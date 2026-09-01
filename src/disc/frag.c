/*
 * AtlasPS2 - frag.c
 * Turning a path into the list of device sectors its data lives in.
 *
 * A small read-only FAT16/FAT32 reader. It exists because the drive
 * emulation must not call a filesystem while a game is running (see
 * frag.h), so the filesystem is read once, here, through a callback -
 * which also makes the whole thing checkable on the build machine
 * against volumes built byte by byte in memory.
 *
 * Read-only, and only what is needed to answer "where is this file":
 * no writing, no directory creation, no timestamps. Anything this does
 * not understand is refused rather than guessed at, because a guess
 * produces an extent list that reads another file's data with nothing
 * to say it did.
 */
#include <string.h>

#include "atlas/frag.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* On-disk layout                                                      */
/*                                                                     */
/* Every multi-byte field is little-endian and read byte at a time. On */
/* the R3000 a u32 load from an odd address is an address error, not a */
/* slow read, and these structures are packed with no regard for it.   */
/* ------------------------------------------------------------------ */

#define BPB_BYTES_PER_SEC   11
#define BPB_SEC_PER_CLUS    13
#define BPB_RSVD_SEC_CNT    14
#define BPB_NUM_FATS        16
#define BPB_ROOT_ENT_CNT    17
#define BPB_TOT_SEC_16      19
#define BPB_FAT_SZ_16       22
#define BPB_TOT_SEC_32      32
#define BPB_FAT_SZ_32       36
#define BPB_ROOT_CLUS       44          /* FAT32 only                  */

#define DIR_ENTRY_SIZE      32
#define DIR_NAME            0
#define DIR_ATTR            11
#define DIR_FST_CLUS_HI     20
#define DIR_FST_CLUS_LO     26
#define DIR_FILE_SIZE       28

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_LONG_NAME      0x0F

#define ENTRY_FREE          0xE5
#define ENTRY_END           0x00

/* The largest sector a device in a PS2 presents. Buffers below are
 * sized for it; a volume claiming more is refused rather than read
 * into a buffer that cannot hold it. */
#define FRAG_SECTOR_MAX     4096

typedef struct {
    atlas_frag_read_fn read;
    void              *ctx;

    u32 sector_size;
    u32 cluster_sectors;
    u32 fat_start;              /**< first FAT sector                  */
    u32 fat_sectors;
    u32 data_start;             /**< sector of cluster 2               */
    u32 cluster_count;
    u32 root_cluster;           /**< FAT32; 0 on FAT16                 */
    u32 root_sector;            /**< FAT16 fixed root; 0 on FAT32      */
    u32 root_entries;           /**< FAT16 only                        */
    int is_fat32;

    /*
     * One sector of the FAT, cached. Walking a cluster chain reads the
     * same FAT sector for 128 consecutive clusters on FAT32, and a file
     * of any size is thousands of clusters: without this the chain walk
     * is one device read per cluster.
     */
    u32           fat_cached;   /**< sector number, or 0xFFFFFFFF      */
    unsigned char fat_buf[FRAG_SECTOR_MAX];

    unsigned char dir_buf[FRAG_SECTOR_MAX];
} fat_t;

#define NO_SECTOR 0xFFFFFFFFU

/* ------------------------------------------------------------------ */

static u16 le16(const unsigned char *p)
{
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static u32 le32(const unsigned char *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static char to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* ------------------------------------------------------------------ */
/* Volume                                                              */
/* ------------------------------------------------------------------ */

static atlas_err_t fat_mount(fat_t *f)
{
    unsigned char boot[FRAG_SECTOR_MAX];
    u32 root_sectors, fat_size, total_sectors, data_sectors;
    u32 reserved, num_fats, root_entries;

    /*
     * The boot sector is read at the device's own sector size, which is
     * not known until it has been read. 512 is what every device this
     * runs on uses for sector 0, and the size the volume declares is
     * checked against it below.
     */
    f->sector_size = 512;

    if (f->read(f->ctx, 0, 1, boot) != 0)
        return ATLAS_EIO;

    f->sector_size = le16(boot + BPB_BYTES_PER_SEC);

    /*
     * A sector size that is not a power of two, or larger than the
     * buffers here, is not something to guess about: every offset below
     * is computed from it.
     */
    if (f->sector_size < 512 || f->sector_size > FRAG_SECTOR_MAX
        || (f->sector_size & (f->sector_size - 1)) != 0) {
        ATLAS_LOG("FRAG", "sector size %u unusable", f->sector_size);
        return ATLAS_EFORMAT;
    }

    f->cluster_sectors = boot[BPB_SEC_PER_CLUS];
    reserved           = le16(boot + BPB_RSVD_SEC_CNT);
    num_fats           = boot[BPB_NUM_FATS];
    root_entries       = le16(boot + BPB_ROOT_ENT_CNT);

    /*
     * Zero in any of these is how a non-FAT sector reads. Checking them
     * together is what distinguishes "this is not a FAT volume" from a
     * FAT volume with something unusual in it - and a division by zero
     * below would otherwise be the first thing to notice.
     */
    if (f->cluster_sectors == 0 || reserved == 0 || num_fats == 0
        || (f->cluster_sectors & (f->cluster_sectors - 1)) != 0) {
        ATLAS_LOG("FRAG", "not a FAT volume");
        return ATLAS_EFORMAT;
    }

    fat_size = le16(boot + BPB_FAT_SZ_16);
    if (fat_size == 0)
        fat_size = le32(boot + BPB_FAT_SZ_32);

    total_sectors = le16(boot + BPB_TOT_SEC_16);
    if (total_sectors == 0)
        total_sectors = le32(boot + BPB_TOT_SEC_32);

    if (fat_size == 0 || total_sectors == 0) {
        ATLAS_LOG("FRAG", "not a FAT volume");
        return ATLAS_EFORMAT;
    }

    /* The root directory on FAT16 is a fixed run of sectors before the
     * data area; on FAT32 it is an ordinary cluster chain and this is
     * zero. Which one a volume is follows from the cluster count, not
     * from any field - that is how FAT is specified. */
    root_sectors = ((u32)root_entries * DIR_ENTRY_SIZE + f->sector_size - 1)
                   / f->sector_size;

    f->fat_start   = reserved;
    f->fat_sectors = fat_size;
    f->data_start  = reserved + num_fats * fat_size + root_sectors;

    if (f->data_start >= total_sectors) {
        ATLAS_LOG("FRAG", "not a FAT volume");
        return ATLAS_EFORMAT;
    }

    data_sectors     = total_sectors - f->data_start;
    f->cluster_count = data_sectors / f->cluster_sectors;

    /* The boundary is the one in Microsoft's specification. It is not a
     * heuristic and must not be adjusted: a volume read as the wrong
     * width walks the chain through the wrong entries. */
    f->is_fat32 = (f->cluster_count >= 65525);

    if (f->is_fat32) {
        f->root_cluster = le32(boot + BPB_ROOT_CLUS);
        f->root_sector  = 0;
        f->root_entries = 0;

        if (f->root_cluster < 2) {
            ATLAS_LOG("FRAG", "FAT32 root cluster %u invalid",
                      f->root_cluster);
            return ATLAS_EFORMAT;
        }
    } else {
        if (root_entries == 0) {
            ATLAS_LOG("FRAG", "not a FAT volume");
            return ATLAS_EFORMAT;
        }

        f->root_cluster = 0;
        f->root_sector  = reserved + num_fats * fat_size;
        f->root_entries = root_entries;
    }

    f->fat_cached = NO_SECTOR;
    return ATLAS_OK;
}

/**
 * The FAT entry for `cluster`.
 *
 * @return the next cluster, 0 on a read failure, or a value >= the
 *         end-of-chain marker for the last cluster of a file. The
 *         caller distinguishes those with fat_is_end().
 */
static u32 fat_next(fat_t *f, u32 cluster)
{
    u32 offset = f->is_fat32 ? cluster * 4 : cluster * 2;
    u32 sector = f->fat_start + offset / f->sector_size;
    u32 within = offset % f->sector_size;

    if (sector >= f->fat_start + f->fat_sectors)
        return 0;

    if (f->fat_cached != sector) {
        if (f->read(f->ctx, sector, 1, f->fat_buf) != 0)
            return 0;

        f->fat_cached = sector;
    }

    if (f->is_fat32) {
        /* The top four bits are reserved and are not part of the
         * cluster number. A volume that has them set is normal. */
        return le32(f->fat_buf + within) & 0x0FFFFFFFU;
    }

    return le16(f->fat_buf + within);
}

static int fat_is_end(const fat_t *f, u32 cluster)
{
    /* Below 2 is "free" or "reserved", which in a chain means the chain
     * is broken - treated as the end, so a damaged file is short rather
     * than a walk that never terminates. */
    if (cluster < 2)
        return 1;

    return f->is_fat32 ? (cluster >= 0x0FFFFFF8U) : (cluster >= 0xFFF8U);
}

static u32 cluster_sector(const fat_t *f, u32 cluster)
{
    return f->data_start + (cluster - 2) * f->cluster_sectors;
}

/* ------------------------------------------------------------------ */
/* Names                                                               */
/* ------------------------------------------------------------------ */

/**
 * Does the 8.3 entry at `e` match the name in [name, name_end)?
 *
 * The comparison is case-insensitive, which is what FAT means by a
 * name: the same file is "Disc.iso" and "DISC.ISO" depending on which
 * program wrote it.
 */
static int match_83(const unsigned char *e, const char *name,
                    const char *name_end)
{
    char built[13];
    int n = 0, i;

    for (i = 0; i < 8 && e[DIR_NAME + i] != ' '; i++)
        built[n++] = (char)e[DIR_NAME + i];

    if (e[DIR_NAME + 8] != ' ') {
        built[n++] = '.';

        for (i = 8; i < 11 && e[DIR_NAME + i] != ' '; i++)
            built[n++] = (char)e[DIR_NAME + i];
    }

    built[n] = '\0';

    if (n != (int)(name_end - name))
        return 0;

    for (i = 0; i < n; i++) {
        if (to_upper(built[i]) != to_upper(name[i]))
            return 0;
    }

    return 1;
}

/**
 * Pull the name fragment out of one long-name entry.
 *
 * A long name is stored backwards across several entries, 13 UTF-16
 * code units at a time, at three disjoint offsets within each. Only the
 * low byte of each unit is taken: a name with characters outside Latin-1
 * will not match here, and the 8.3 alias is what finds it instead.
 */
static int lfn_chars(const unsigned char *e, char *out)
{
    static const int k_off[13] = { 1, 3, 5, 7, 9,
                                   14, 16, 18, 20, 22, 24,
                                   28, 30 };
    int i, n = 0;

    for (i = 0; i < 13; i++) {
        u16 c = le16(e + k_off[i]);

        if (c == 0x0000 || c == 0xFFFF)
            break;

        out[n++] = (c < 0x100) ? (char)c : '?';
    }

    return n;
}

/* ------------------------------------------------------------------ */
/* Directory search                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    u32 cluster;        /**< first cluster of the entry found     */
    u32 size;           /**< its length in bytes                  */
    int is_dir;
} found_t;

/*
 * The assembled long name of the entry currently being examined. It is
 * built across the entries preceding the 8.3 one, and is 260 characters
 * because that is the longest FAT stores.
 */
#define LFN_MAX 260

/**
 * Look for one path component in one directory.
 *
 * @param cluster the directory's first cluster, or 0 for a FAT16 root.
 */
static atlas_err_t dir_find(fat_t *f, u32 cluster, const char *name,
                            const char *name_end, found_t *out)
{
    char lfn[LFN_MAX + 1];
    int  lfn_len = 0;
    u32  sector, remaining_entries = 0;
    int  in_root16 = (cluster == 0 && !f->is_fat32);
    u32  sectors_left;

    if (in_root16) {
        sector = f->root_sector;
        remaining_entries = f->root_entries;
        sectors_left = (remaining_entries * DIR_ENTRY_SIZE + f->sector_size - 1)
                       / f->sector_size;
    } else {
        if (cluster == 0)
            cluster = f->root_cluster;

        sector = cluster_sector(f, cluster);
        sectors_left = f->cluster_sectors;
    }

    for (;;) {
        u32 i;

        if (f->read(f->ctx, sector, 1, f->dir_buf) != 0)
            return ATLAS_EIO;

        for (i = 0; i + DIR_ENTRY_SIZE <= f->sector_size; i += DIR_ENTRY_SIZE) {
            const unsigned char *e = f->dir_buf + i;
            unsigned char attr = e[DIR_ATTR];

            if (e[DIR_NAME] == ENTRY_END)
                return ATLAS_ENOENT;     /* nothing after this point */

            if (e[DIR_NAME] == ENTRY_FREE) {
                lfn_len = 0;
                continue;
            }

            if ((attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
                /*
                 * Fragments arrive last-first, so each is prepended.
                 * A sequence number the entry itself carries would let
                 * this validate the order; it is not checked, because
                 * an out-of-order long name simply fails to match and
                 * the 8.3 alias below still finds the file.
                 */
                char part[16];
                int  n = lfn_chars(e, part);

                if (lfn_len + n <= LFN_MAX) {
                    memmove(lfn + n, lfn, (size_t)lfn_len);
                    memcpy(lfn, part, (size_t)n);
                    lfn_len += n;
                } else {
                    lfn_len = 0;         /* too long to be our name */
                }

                continue;
            }

            if (attr & ATTR_VOLUME_ID) {
                lfn_len = 0;
                continue;
            }

            /* A match on either name. The long name is checked first
             * because it is what the user sees. */
            {
                int hit = 0;
                int want = (int)(name_end - name);

                if (lfn_len == want) {
                    int k;

                    hit = 1;
                    for (k = 0; k < want; k++) {
                        if (to_upper(lfn[k]) != to_upper(name[k])) {
                            hit = 0;
                            break;
                        }
                    }
                }

                if (!hit)
                    hit = match_83(e, name, name_end);

                if (hit) {
                    out->cluster = ((u32)le16(e + DIR_FST_CLUS_HI) << 16)
                                 | le16(e + DIR_FST_CLUS_LO);
                    out->size    = le32(e + DIR_FILE_SIZE);
                    out->is_dir  = (attr & ATTR_DIRECTORY) != 0;
                    return ATLAS_OK;
                }
            }

            lfn_len = 0;
        }

        /* On to the next sector, and on FAT32 the next cluster when
         * this one runs out. */
        sector++;

        if (--sectors_left > 0)
            continue;

        if (in_root16)
            return ATLAS_ENOENT;

        cluster = fat_next(f, cluster);

        if (cluster == 0)
            return ATLAS_EIO;

        if (fat_is_end(f, cluster))
            return ATLAS_ENOENT;

        sector = cluster_sector(f, cluster);
        sectors_left = f->cluster_sectors;
    }
}

/* ------------------------------------------------------------------ */
/* Building the list                                                   */
/* ------------------------------------------------------------------ */

/**
 * Walk the cluster chain, coalescing consecutive clusters into runs.
 *
 * The coalescing is the point. A file written in one go occupies
 * thousands of consecutive clusters, and recording each separately
 * would overflow the table on a file that is not fragmented at all.
 */
static atlas_err_t build_runs(fat_t *f, u32 cluster, u32 size,
                              atlas_fraglist_t *out)
{
    u32 want_sectors = (size + f->sector_size - 1) / f->sector_size;
    u32 have_sectors = 0;

    out->count       = 0;
    out->sector_size = f->sector_size;
    out->size        = size;

    while (!fat_is_end(f, cluster) && have_sectors < want_sectors) {
        u32 sector = cluster_sector(f, cluster);
        u32 next;

        if (cluster - 2 >= f->cluster_count) {
            ATLAS_LOG("FRAG", "cluster %u is outside the volume", cluster);
            return ATLAS_EFORMAT;
        }

        /* Extend the run in place when this cluster follows the last
         * one, otherwise start a new run. */
        if (out->count > 0
            && out->frag[out->count - 1].start
               + out->frag[out->count - 1].count == sector) {
            out->frag[out->count - 1].count += f->cluster_sectors;
        } else {
            if (out->count >= ATLAS_FRAG_MAX) {
                /*
                 * Refused rather than truncated. A short list reads
                 * whatever is at the sectors it does have and returns
                 * it as the file, which is data from somewhere else on
                 * the card presented as the game's own.
                 */
                ATLAS_LOG("FRAG", "file needs more than %d runs",
                          ATLAS_FRAG_MAX);
                return ATLAS_ENOMEM;
            }

            out->frag[out->count].start = sector;
            out->frag[out->count].count = f->cluster_sectors;
            out->count++;
        }

        have_sectors += f->cluster_sectors;

        next = fat_next(f, cluster);
        if (next == 0)
            return ATLAS_EIO;

        cluster = next;
    }

    if (have_sectors < want_sectors) {
        /* The chain ended before the size in the directory entry was
         * accounted for. The file on the card is damaged, and saying so
         * is better than handing back a list that is quietly short. */
        ATLAS_LOG("FRAG", "chain ends %u sectors early",
                  want_sectors - have_sectors);
        return ATLAS_EFORMAT;
    }

    return ATLAS_OK;
}

static atlas_err_t frag_build(atlas_frag_read_fn read, void *ctx,
                              const char *path, atlas_fraglist_t *out)
{
    static fat_t f;     /* ~12 KB; not something to put on a stack */
    atlas_err_t err;
    const char *p;
    u32 cluster;

    if (!read || !path || !out)
        return ATLAS_EINVAL;

    out->count = 0;

    memset(&f, 0, sizeof(f));
    f.read = read;
    f.ctx  = ctx;

    err = fat_mount(&f);
    if (err != ATLAS_OK)
        return err;

    /* Start at the root; on FAT16 that is signalled by cluster 0. */
    cluster = f.is_fat32 ? f.root_cluster : 0;

    p = path;
    while (*p == '/' || *p == '\\')
        p++;

    if (!*p)
        return ATLAS_ENOENT;    /* the root is not a file */

    for (;;) {
        const char *end = p;
        found_t     hit;

        while (*end && *end != '/' && *end != '\\')
            end++;

        if (end == p)
            return ATLAS_ENOENT;

        err = dir_find(&f, cluster, p, end, &hit);
        if (err != ATLAS_OK)
            return err;

        /* Skip the separators to see whether anything follows. */
        p = end;
        while (*p == '/' || *p == '\\')
            p++;

        if (!*p) {
            if (hit.is_dir)
                return ATLAS_ENOENT;    /* asked for a file, found a directory */

            if (hit.size == 0)
                return ATLAS_EFORMAT;

            return build_runs(&f, hit.cluster, hit.size, out);
        }

        if (!hit.is_dir)
            return ATLAS_ENOENT;        /* a path component that is a file */

        cluster = hit.cluster;

        /* On FAT16 a subdirectory's "." entry gives cluster 0 for the
         * root; every other directory has a real cluster. */
        if (cluster == 0 && f.is_fat32)
            cluster = f.root_cluster;
    }
}

atlas_err_t atlas_frag_build(atlas_frag_read_fn read, void *ctx,
                             const char *path, atlas_fraglist_t *out)
{
    atlas_err_t err = frag_build(read, ctx, path, out);

    /*
     * Nothing partial ever leaves here. build_runs() can fail after
     * recording most of a chain - a damaged file, a card that goes away
     * mid-walk - and a caller that read through what was left would get
     * real sectors belonging to something else, which is the failure
     * this module exists to avoid. An empty list makes every lookup
     * refuse instead.
     */
    if (err != ATLAS_OK && out) {
        out->count = 0;
        out->size  = 0;
    }

    return err;
}

/* ------------------------------------------------------------------ */
/* Lookup                                                              */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_frag_lookup(const atlas_fraglist_t *fl, u32 offset,
                              u32 *out_sector, u32 *out_skip, u32 *out_run)
{
    u32 seen = 0;
    int i;

    if (!fl || !out_sector || !out_skip || !out_run)
        return ATLAS_EINVAL;

    if (offset >= fl->size)
        return ATLAS_EINVAL;

    for (i = 0; i < fl->count; i++) {
        u32 bytes = fl->frag[i].count * fl->sector_size;

        if (offset < seen + bytes) {
            u32 within = offset - seen;

            *out_sector = fl->frag[i].start + within / fl->sector_size;
            *out_skip   = within % fl->sector_size;
            *out_run    = bytes - within;

            /* The run must not promise past the end of the file: the
             * last cluster is usually only partly used, and a caller
             * reading the whole of it would return slack as data. */
            if (*out_run > fl->size - offset)
                *out_run = fl->size - offset;

            return ATLAS_OK;
        }

        seen += bytes;
    }

    /* The runs hold at least `size` bytes - build_runs() refuses a
     * chain that does not - so this is unreachable unless the list was
     * built by something else. */
    return ATLAS_EINVAL;
}
