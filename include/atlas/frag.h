/*
 * AtlasPS2 - frag.h
 *
 * A file, reduced to the list of raw device sectors it occupies.
 *
 * WHY NOT JUST OPEN THE FILE
 * --------------------------
 * Once a game is running it owns the IOP. Its own modules are loaded,
 * its threads run at priorities it chose, and it programs DMA channels
 * whenever it likes. Calling into a FAT driver from inside the drive
 * emulation at that point means running thousands of lines of somebody
 * else's code - allocating, taking semaphores, touching a cache - on an
 * interrupt path the game can pre-empt, in memory the game may have
 * decided is its own.
 *
 * So the filesystem is used exactly once, before the game starts, to
 * answer a single question: which sectors of the device is this file
 * made of? After that the drive emulation reads those sectors directly
 * and never touches a filesystem again.
 *
 * This is also why the answer is a small fixed table rather than a
 * chain walked on demand: walking the FAT during a read would be
 * another device access in the middle of the one the game is waiting
 * for.
 *
 * WHAT A WRONG ANSWER COSTS
 * -------------------------
 * An extent that points one sector short returns real data, from the
 * wrong place, with no error anywhere - which is exactly the failure
 * the ISO/ZSO comparison in test_image was built to catch, and it is
 * caught the same way here: the extents are checked against the bytes
 * the ordinary file API returns for the same file.
 */
#ifndef ATLAS_FRAG_H
#define ATLAS_FRAG_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One run of consecutive device sectors.
 *
 * `count` is in device sectors, not bytes: the drive emulation hands
 * these straight to a block device's read function, which counts in
 * sectors.
 */
typedef struct {
    u32 start;      /**< first device sector */
    u32 count;      /**< how many, consecutive */
} atlas_frag_t;

/*
 * How many runs a file may be broken into.
 *
 * A freshly copied image on a freshly formatted card is one run. A
 * card that has been filled and emptied a few times gives tens. 512 is
 * chosen so the table costs 4 KB, which is affordable to keep resident
 * on the IOP for the life of a game - and a file more fragmented than
 * this is refused rather than partly loaded, because a partial extent
 * list reads another file's data with no error to show for it.
 */
#define ATLAS_FRAG_MAX 512

typedef struct {
    atlas_frag_t frag[ATLAS_FRAG_MAX];
    int          count;         /**< runs used */

    u32          sector_size;   /**< device sector size, in bytes */
    u32          size;          /**< the file's length in bytes */
} atlas_fraglist_t;

/**
 * Read `count` device sectors starting at `sector`.
 *
 * The same shape as a block device's read, so the caller can pass one
 * through directly. Returns 0 on success.
 */
typedef int (*atlas_frag_read_fn)(void *ctx, u32 sector, u32 count, void *buf);

/**
 * Find `path` on a FAT16 or FAT32 volume and record where its data is.
 *
 * `path` is absolute within the volume, with either separator:
 * "/GAMES/DISC.ISO". Long names are matched as written; the short 8.3
 * alias is matched too, so a file the user renamed on a PC and a file
 * written by something that never made a long name both resolve.
 *
 * `read` sees sector numbers relative to the start of the volume - the
 * caller is expected to have added any partition offset, which is what
 * a block device's own read already does.
 *
 * @return ATLAS_OK, ATLAS_ENOENT if the path is not there, ATLAS_EFORMAT
 *         if the volume is not FAT, ATLAS_ENOMEM if the file needs more
 *         than ATLAS_FRAG_MAX runs, ATLAS_EIO on a read failure.
 */
atlas_err_t atlas_frag_build(atlas_frag_read_fn read, void *ctx,
                             const char *path, atlas_fraglist_t *out);

/**
 * Where in the device is byte `offset` of the file?
 *
 * @param out_sector   receives the device sector holding it
 * @param out_skip     receives the byte offset within that sector
 * @param out_run      receives how many bytes are consecutive on the
 *                     device from there - which is what lets a caller
 *                     read many sectors in one request instead of one
 *                     at a time.
 *
 * @return ATLAS_OK, or ATLAS_EINVAL if `offset` is past the end of the
 *         file. Never a silent zero: a read past the end that returned
 *         zeroes would look to a game like a disc that is merely empty.
 */
atlas_err_t atlas_frag_lookup(const atlas_fraglist_t *fl, u32 offset,
                              u32 *out_sector, u32 *out_skip, u32 *out_run);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_FRAG_H */
