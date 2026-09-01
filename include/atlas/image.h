/*
 * AtlasPS2 - image.h
 *
 * A disc image as a thing you can read sectors from, whatever file
 * format it happens to be in.
 *
 * WHY THIS SITS BETWEEN disc.h AND THE FILESYSTEM
 * ----------------------------------------------
 * disc.h answers "what game is this" from sectors; it does not know
 * what a file is. The IOP-side loader, later, needs the same sectors at
 * DMA speed. Both want one question answered - give me sector N - and
 * neither should care whether the bytes came from a plain ISO or from a
 * compressed block that had to be inflated first.
 *
 * So the format handling lives here, once. Adding a format means adding
 * an opener here and nothing anywhere else.
 *
 * WHICH FORMATS, AND WHY NOT THE OTHERS
 * -------------------------------------
 * ISO is the whole disc, byte for byte, and needs no interpretation.
 *
 * ZSO is that same disc cut into fixed blocks, each compressed with
 * LZ4, with a table of offsets at the front. It earns its place because
 * decompression is a byte-copy loop with no entropy decoding and no
 * window state: a block can be inflated in isolation, at the speed a
 * game reading its own data actually needs.
 *
 * CSO is the same shape with DEFLATE instead. It compresses better and
 * cannot be decoded fast enough on a 37 MHz IOP to keep a game's
 * streaming audio fed, which is why it is not here.
 *
 * CHD is not here either, and for a different reason: its hunk map is
 * a compressed structure of its own, it mixes several codecs within one
 * file, and its metadata assumes a CD framing this console's drive does
 * not use. It is a preservation format, not a playback one.
 */
#ifndef ATLAS_IMAGE_H
#define ATLAS_IMAGE_H

#include "atlas/atlas.h"
#include "atlas/disc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ATLAS_IMAGE_ISO = 0,   /**< raw 2048-byte sectors                  */
    ATLAS_IMAGE_ZSO        /**< LZ4-compressed blocks with an index    */
} atlas_image_format_t;

/*
 * The index of a ZSO is one 32-bit entry per block, and it is held in
 * RAM for the life of the image: a seek that had to read the index from
 * the card first would double every access. A 4.7 GB DVD at the usual
 * 2 KB block size needs 2.3 million entries - 9 MB, a quarter of the
 * console's memory, for a table that is consulted twice a second.
 *
 * So the index is capped and larger images are refused with a clear
 * error rather than accepted and then failing at an unpredictable
 * moment. At the 16 KB block size ZSO tools use for DVDs, this covers
 * a full dual-layer disc; the images that hit it are the ones built
 * with a CD-sized block, which are the ones that would have been slow
 * anyway.
 */
#define ATLAS_IMAGE_INDEX_MAX 65536

typedef struct atlas_image atlas_image_t;

/**
 * Open a disc image. The format is detected from the file's contents,
 * never from its extension: a renamed file is a normal thing to find on
 * a user's drive, and the header is the only honest answer.
 *
 * @return ATLAS_OK; ATLAS_ENOENT if it cannot be opened; ATLAS_EFORMAT
 *         if it is not a format this understands; ATLAS_ENOMEM if its
 *         index is larger than ATLAS_IMAGE_INDEX_MAX; ATLAS_EIO on a
 *         read failure.
 */
atlas_err_t atlas_image_open(const char *path, atlas_image_t **out);

/** Close an image and release its index. NULL is accepted. */
void atlas_image_close(atlas_image_t *img);

/**
 * Read `count` consecutive 2048-byte sectors.
 *
 * Matches atlas_disc_read_fn, so an open image can be handed straight
 * to atlas_disc_probe().
 *
 * A read past the end of the volume fails rather than returning zeroes.
 * Zeroes parse as a valid empty directory, which turns a truncated
 * download into a disc that merely looks like it has nothing on it.
 *
 * @return 0 on success, negative on failure.
 */
int atlas_image_read(void *img, u32 lba, u32 count, void *buf);

/** Total number of 2048-byte sectors in the image. */
u32 atlas_image_sectors(const atlas_image_t *img);

/** Which format the file turned out to be. */
atlas_image_format_t atlas_image_format(const atlas_image_t *img);

/**
 * Decompress one LZ4 block.
 *
 * Exposed for the self-check. LZ4 is the one piece of this file that is
 * pure arithmetic over bytes, it is the piece that runs on every sector
 * a game reads, and a malformed block is attacker-controlled input in
 * the sense that matters here: an image from the internet, decoded into
 * a fixed buffer. Every offset and length in the stream is checked
 * against both ends before it is used.
 *
 * @return the number of bytes written, or negative if the block is
 *         malformed or would not fit in `dst_size`.
 */
int atlas_lz4_decompress(const void *src, int src_size,
                         void *dst, int dst_size);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_IMAGE_H */
