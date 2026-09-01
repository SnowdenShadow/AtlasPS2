/*
 * AtlasPS2 - image.c
 * A disc image file, presented as sectors.
 *
 * The format handling lives here and nowhere else. Above this, disc.c
 * asks for sector N and gets it; below, a ZSO is a table of offsets
 * and a pile of LZ4 blocks, and an ISO is nothing at all.
 */
#include <string.h>
#include <stdlib.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

#include "atlas/image.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* ZSO layout                                                          */
/*                                                                     */
/* The header is 24 bytes, then one 32-bit index entry per block plus  */
/* a terminator, then the blocks themselves. An entry's high bit marks */
/* a block stored uncompressed - which happens whenever compressing it */
/* made it bigger, and which for a disc full of encrypted data is most */
/* of the file.                                                        */
/* ------------------------------------------------------------------ */

#define ZSO_MAGIC          0x4F53495AU   /* 'ZISO', little-endian      */

#define ZSO_OFF_MAGIC      0
#define ZSO_OFF_HDR_SIZE   4
#define ZSO_OFF_TOTAL_SIZE 8             /* uncompressed, 64-bit       */
#define ZSO_OFF_BLOCK_SIZE 16
#define ZSO_OFF_VERSION    20
#define ZSO_OFF_ALIGN      21
#define ZSO_HEADER_SIZE    24

#define ZSO_INDEX_UNCOMPRESSED 0x80000000U

/*
 * The block size a ZSO was built with. 2 KB is one sector, 16 KB is
 * what the common tools use for DVDs, and 64 KB is the largest anything
 * produces. Anything outside this range is either not a ZSO or was
 * built by something this has never seen, and guessing would mean
 * decompressing into a buffer of the wrong size.
 */
#define ZSO_BLOCK_MIN      2048
#define ZSO_BLOCK_MAX      65536

/* ------------------------------------------------------------------ */
/* The image                                                           */
/* ------------------------------------------------------------------ */

struct atlas_image {
    int                   fd;
    atlas_image_format_t  format;
    u32                   sectors;      /**< in 2048-byte units        */
    u32                   file_size;    /**< bytes actually on disk    */

    /* ZSO only */
    u32  block_size;
    int  sectors_per_block;
    u32  align;                         /**< offset shift, usually 0   */
    u32 *index;                         /**< block_count + 1 entries   */
    u32  block_count;

    /*
     * The most recently decompressed block, kept because a game - and
     * ISO9660 itself - reads several consecutive sectors out of the
     * same block. Without it, reading a 16 KB block's eight sectors one
     * at a time decompresses the same block eight times.
     */
    u32  cached_block;                  /**< index, or 0xFFFFFFFF      */
    unsigned char *cache;
    unsigned char *scratch;             /**< the compressed block      */
};

#define NO_BLOCK 0xFFFFFFFFU

/*
 * These are allocated per open image rather than statically: only one
 * image is open at a time today, but a static pair would silently
 * corrupt both if that ever stopped being true, and the failure would
 * appear as a game reading another game's data.
 */

static u32 le32(const unsigned char *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* ------------------------------------------------------------------ */
/* Raw file access                                                     */
/* ------------------------------------------------------------------ */

static int read_at(int fd, u32 offset, void *buf, int len)
{
    if (fileXioLseek(fd, (int)offset, SEEK_SET) < 0)
        return -1;

    if (fileXioRead(fd, buf, len) != len)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* ZSO                                                                 */
/* ------------------------------------------------------------------ */

/**
 * Read the header and index of a ZSO.
 *
 * The index is what makes random access possible, so it is read once
 * and held: consulting it per sector from the card would double the
 * cost of every access on a device where a seek is already the
 * expensive part.
 */
static atlas_err_t zso_open(atlas_image_t *img, const unsigned char *hdr)
{
    u64 total;
    u32 index_bytes;
    int i;

    img->block_size = le32(hdr + ZSO_OFF_BLOCK_SIZE);
    img->align      = hdr[ZSO_OFF_ALIGN];

    /*
     * A block size that is not a whole number of sectors would mean a
     * sector spanning two blocks, and every read would need two
     * decompressions and a join. No tool produces such a file, so it is
     * refused rather than supported: the alternative is a rarely-taken
     * path that nothing ever tests.
     */
    if (img->block_size < ZSO_BLOCK_MIN || img->block_size > ZSO_BLOCK_MAX
        || (img->block_size % ATLAS_DISC_SECTOR_SIZE) != 0) {
        ATLAS_LOG("IMG", "ZSO block size %u unusable", img->block_size);
        return ATLAS_EFORMAT;
    }

    img->sectors_per_block = (int)(img->block_size / ATLAS_DISC_SECTOR_SIZE);

    /* The uncompressed size is 64-bit in the header. A PS2 disc tops
     * out at 8.5 GB, but a corrupt field could claim far more, and the
     * sector count below must not silently wrap. */
    total = (u64)le32(hdr + ZSO_OFF_TOTAL_SIZE)
          | ((u64)le32(hdr + ZSO_OFF_TOTAL_SIZE + 4) << 32);

    if (total == 0 || total > ((u64)1 << 34)) {
        ATLAS_LOG("IMG", "ZSO claims an impossible size");
        return ATLAS_EFORMAT;
    }

    img->sectors     = (u32)(total / ATLAS_DISC_SECTOR_SIZE);
    img->block_count = (u32)((total + img->block_size - 1) / img->block_size);

    /*
     * The index is held in RAM for the life of the image. A DVD cut
     * into 2 KB blocks needs millions of entries and would take a
     * quarter of the console's memory, so the cap refuses those up
     * front with an error the user can act on, rather than allocating
     * until something else fails in a way they cannot.
     */
    if (img->block_count + 1 > ATLAS_IMAGE_INDEX_MAX) {
        ATLAS_LOG("IMG", "ZSO index too large (%u blocks)", img->block_count);
        return ATLAS_ENOMEM;
    }

    index_bytes = (img->block_count + 1) * 4;

    img->index = (u32 *)malloc(index_bytes);
    if (!img->index)
        return ATLAS_ENOMEM;

    if (read_at(img->fd, ZSO_HEADER_SIZE, img->index, (int)index_bytes) != 0) {
        ATLAS_LOG("IMG", "ZSO index unreadable");
        return ATLAS_EIO;
    }

    /*
     * The index arrives as little-endian bytes and is used as u32s.
     * The EE is little-endian so this is a no-op today, but reading it
     * back through le32() rather than casting keeps the file format
     * independent of the host - and costs one pass over a table that is
     * consulted for the life of the image.
     */
    for (i = 0; i <= (int)img->block_count; i++) {
        const unsigned char *p = (const unsigned char *)&img->index[i];
        img->index[i] = le32(p);
    }

    img->cache = (unsigned char *)malloc(img->block_size);

    /*
     * The scratch buffer holds a block as stored. An incompressible
     * block is stored slightly larger than its uncompressed form -
     * LZ4's worst case adds a byte per 255 - so sizing it at the block
     * size would fail on exactly the encrypted-data blocks that most of
     * a real disc consists of.
     */
    img->scratch = (unsigned char *)malloc(img->block_size
                                           + img->block_size / 255 + 64);

    if (!img->cache || !img->scratch)
        return ATLAS_ENOMEM;

    img->cached_block = NO_BLOCK;
    img->format = ATLAS_IMAGE_ZSO;

    return ATLAS_OK;
}

/**
 * Make block `n` current in the cache.
 *
 * @return ATLAS_OK, ATLAS_EIO on a read failure, ATLAS_EFORMAT if the
 *         block does not decompress to the size the header promised.
 */
static atlas_err_t zso_load_block(atlas_image_t *img, u32 n)
{
    u32 start, end, offset, stored;
    int uncompressed;
    int want;

    if (img->cached_block == n)
        return ATLAS_OK;

    if (n >= img->block_count)
        return ATLAS_EINVAL;

    start = img->index[n];
    end   = img->index[n + 1];

    uncompressed = (start & ZSO_INDEX_UNCOMPRESSED) != 0;

    offset = (start & ~ZSO_INDEX_UNCOMPRESSED) << img->align;
    stored = (end   & ~ZSO_INDEX_UNCOMPRESSED) << img->align;

    /*
     * A block's stored length is the distance to the next block's
     * offset. A corrupt index can make that negative or absurd, and
     * both would become a read length - so it is checked here rather
     * than trusted into fileXioRead().
     */
    if (stored <= offset)
        return ATLAS_EFORMAT;

    stored -= offset;

    /*
     * Two ways the stored length overstates what is actually there, and
     * both are ordinary rather than corrupt.
     *
     * The scratch buffer bounds it from above: a length longer than the
     * worst case for a whole block cannot be a block.
     *
     * The end of the file bounds it too. A writer that aligns its
     * blocks rounds the terminator entry up past the last byte it
     * wrote, so the final block's computed length runs past EOF. The
     * padding is not data, and reading short there is correct - which
     * is why the read below asks for what remains rather than failing
     * when it cannot have it all.
     */
    if (offset >= img->file_size)
        return ATLAS_EFORMAT;

    want = (int)stored;
    if (want > (int)(img->block_size + img->block_size / 255 + 64))
        want = (int)(img->block_size + img->block_size / 255 + 64);

    if (offset + (u32)want > img->file_size)
        want = (int)(img->file_size - offset);

    if (read_at(img->fd, offset, img->scratch, want) != 0) {
        ATLAS_LOG("IMG", "block %u unreadable", n);
        return ATLAS_EIO;
    }

    if (uncompressed) {
        /*
         * Stored as-is because compressing it made it larger. Copy only
         * what a block holds: the slack after it is alignment padding,
         * not data.
         */
        int n_copy = want;

        if (n_copy > (int)img->block_size)
            n_copy = (int)img->block_size;

        memcpy(img->cache, img->scratch, (size_t)n_copy);

        /* A short final block leaves the tail undefined otherwise, and
         * a game reading it would get whatever the previous block put
         * there - a difference between runs, which is the hardest kind
         * of bug to see. */
        if (n_copy < (int)img->block_size)
            memset(img->cache + n_copy, 0, img->block_size - (size_t)n_copy);
    } else {
        int got = atlas_lz4_decompress(img->scratch, want,
                                       img->cache, (int)img->block_size);

        if (got < 0) {
            ATLAS_LOG("IMG", "block %u is corrupt", n);
            return ATLAS_EFORMAT;
        }

        if (got < (int)img->block_size)
            memset(img->cache + got, 0, img->block_size - (size_t)got);
    }

    img->cached_block = n;
    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* Opening                                                             */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_image_open(const char *path, atlas_image_t **out)
{
    unsigned char hdr[ZSO_HEADER_SIZE];
    atlas_image_t *img;
    atlas_err_t err;
    int size;

    if (!path || !path[0] || !out)
        return ATLAS_EINVAL;

    *out = NULL;

    img = (atlas_image_t *)malloc(sizeof(*img));
    if (!img)
        return ATLAS_ENOMEM;

    memset(img, 0, sizeof(*img));
    img->cached_block = NO_BLOCK;

    img->fd = fileXioOpen(path, FIO_O_RDONLY);
    if (img->fd < 0) {
        free(img);
        return ATLAS_ENOENT;
    }

    size = fileXioLseek(img->fd, 0, SEEK_END);
    if (size <= 0) {
        atlas_image_close(img);
        return ATLAS_EFORMAT;
    }

    img->file_size = (u32)size;

    if (read_at(img->fd, 0, hdr, (int)sizeof(hdr)) != 0) {
        atlas_image_close(img);
        return ATLAS_EIO;
    }

    /*
     * The format comes from the header, never from the extension. A
     * renamed file is an ordinary thing to find on a user's drive, and
     * trusting the name means handing a ZSO's compressed bytes to the
     * ISO path, where they parse as a disc with no volume descriptor.
     */
    if (le32(hdr + ZSO_OFF_MAGIC) == ZSO_MAGIC) {
        err = zso_open(img, hdr);
        if (err != ATLAS_OK) {
            atlas_image_close(img);
            return err;
        }
    } else {
        img->format  = ATLAS_IMAGE_ISO;
        img->sectors = (u32)size / ATLAS_DISC_SECTOR_SIZE;

        /*
         * Too small to hold a volume descriptor set. Refusing here
         * means a truncated download is reported as a bad file rather
         * than as a disc whose directory happens to be unreadable.
         */
        if (img->sectors <= 16) {
            atlas_image_close(img);
            return ATLAS_EFORMAT;
        }
    }

    *out = img;
    return ATLAS_OK;
}

void atlas_image_close(atlas_image_t *img)
{
    if (!img)
        return;

    if (img->fd >= 0)
        fileXioClose(img->fd);

    free(img->index);
    free(img->cache);
    free(img->scratch);
    free(img);
}

/* ------------------------------------------------------------------ */
/* Reading                                                             */
/* ------------------------------------------------------------------ */

int atlas_image_read(void *ctx, u32 lba, u32 count, void *buf)
{
    atlas_image_t *img = (atlas_image_t *)ctx;
    unsigned char *dst = (unsigned char *)buf;
    u32 i;

    if (!img || !buf || count == 0)
        return -1;

    /*
     * Past the end of the volume is a failure, never zeroes. A
     * zero-filled buffer parses as a valid empty directory, which turns
     * a truncated image into a disc that merely looks empty - and the
     * user goes looking for a bad dump instead of a bad download.
     */
    if (lba + count > img->sectors || lba + count < lba)
        return -1;

    if (img->format == ATLAS_IMAGE_ISO) {
        return read_at(img->fd, lba * ATLAS_DISC_SECTOR_SIZE,
                       dst, (int)(count * ATLAS_DISC_SECTOR_SIZE));
    }

    for (i = 0; i < count; i++) {
        u32 abs   = lba + i;
        u32 block = abs / (u32)img->sectors_per_block;
        u32 slot  = abs % (u32)img->sectors_per_block;

        if (zso_load_block(img, block) != ATLAS_OK)
            return -1;

        memcpy(dst + i * ATLAS_DISC_SECTOR_SIZE,
               img->cache + slot * ATLAS_DISC_SECTOR_SIZE,
               ATLAS_DISC_SECTOR_SIZE);
    }

    return 0;
}

u32 atlas_image_sectors(const atlas_image_t *img)
{
    return img ? img->sectors : 0;
}

atlas_image_format_t atlas_image_format(const atlas_image_t *img)
{
    return img ? img->format : ATLAS_IMAGE_ISO;
}
