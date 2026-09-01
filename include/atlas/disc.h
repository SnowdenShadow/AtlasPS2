/*
 * AtlasPS2 - disc.h
 *
 * Reading a PlayStation 2 disc image: what it is, and what it says it
 * wants to be booted as.
 *
 * WHY THIS IS A SEPARATE LAYER
 * ----------------------------
 * Everything above this file - the browser row, the compatibility
 * database, the video mode the drive is told to report - is decided
 * from three facts: the title on the volume, the boot path in
 * SYSTEM.CNF, and the game ID that path contains. All three come out of
 * ISO9660 structures that are pure arithmetic over bytes. So this
 * module reads through a callback and never touches fileXio, which
 * means `make check` on the build machine can feed it a synthetic image
 * and pin the answers. The alternative is finding out that a game ID
 * was parsed wrong when a console shows a black screen.
 *
 * WHAT IT DOES NOT DO
 * -------------------
 * It does not mount, cache or stream. A game reading its own data does
 * so through the IOP at DMA speed, and nothing in this file is in that
 * path - this runs once, before launch, to answer "what disc is this".
 */
#ifndef ATLAS_DISC_H
#define ATLAS_DISC_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A CD or DVD data sector as ISO9660 sees it. Physical CD sectors are
 * 2352 bytes, but every image format this launcher accepts hands over
 * the 2048-byte user area already extracted, and the PS2's own drive
 * reports 2048 for data tracks. An image whose logical block size is
 * anything else is not something this console boots.
 */
#define ATLAS_DISC_SECTOR_SIZE 2048

/* "SLUS-20902" is ten characters. The margin is for the malformed
 * files that exist in the wild, which are truncated rather than
 * overrunning anything. */
#define ATLAS_DISC_ID_MAX     16

/* The BOOT2 value verbatim, e.g. "cdrom0:\SLUS_209.02;1". Kept whole
 * because the IOP-side loader is handed this string, not the derived
 * ID: a handful of discs boot a path that is not their ID at all. */
#define ATLAS_DISC_BOOT_MAX   64

/* ISO9660's volume identifier field is 32 bytes, fixed. */
#define ATLAS_DISC_VOLUME_MAX 33

/*
 * The three regional families a PS2 disc belongs to.
 *
 * This is not cosmetic. It decides the video mode the emulated drive
 * must report, and a PAL game told it is running on an NTSC console
 * either refuses to start or runs 20% fast with the bottom of the
 * screen cut off.
 */
typedef enum {
    ATLAS_REGION_UNKNOWN = 0,
    ATLAS_REGION_NTSC_U,      /* SLUS, SCUS, PBPX ... North America   */
    ATLAS_REGION_NTSC_J,      /* SLPS, SLPM, SCPS, SLKA ... Japan/Asia */
    ATLAS_REGION_PAL          /* SLES, SCES, SCED ... Europe          */
} atlas_region_t;

typedef struct {
    /** Normalised game ID, "SLUS-20902". Empty if SYSTEM.CNF gave none. */
    char id[ATLAS_DISC_ID_MAX];

    /** The BOOT2 value exactly as the disc wrote it. Empty if absent. */
    char boot[ATLAS_DISC_BOOT_MAX];

    /** ISO9660 volume identifier, trailing padding removed. May be "". */
    char volume[ATLAS_DISC_VOLUME_MAX];

    /** Derived from `id`, corrected by SYSTEM.CNF's VMODE when present. */
    atlas_region_t region;

    /** Volume size in 2048-byte sectors, from the descriptor. */
    u32 sectors;
} atlas_disc_info_t;

/**
 * Read `count` consecutive 2048-byte sectors starting at `lba`.
 *
 * The seam that keeps this module host-testable. On the console it is a
 * seek and a read on an open image file; in the self-check it is a
 * memcpy out of a synthetic image.
 *
 * A short read must be reported as failure, not as zeroes: a zero-filled
 * buffer parses as a perfectly valid empty directory, and the difference
 * between "this disc has no SYSTEM.CNF" and "the USB stick stopped
 * responding" is one the user needs told.
 *
 * @return 0 on success, negative on any failure.
 */
typedef int (*atlas_disc_read_fn)(void *ctx, u32 lba, u32 count, void *buf);

/**
 * Identify a disc image.
 *
 * Reads the Primary Volume Descriptor, walks the root directory for
 * SYSTEM.CNF, and parses the BOOT2 line out of it.
 *
 * A disc with a valid volume descriptor but no usable SYSTEM.CNF is
 * still reported as success with an empty `id`: that describes several
 * legitimate discs (and every non-game data disc), and the caller can
 * show it while refusing to launch it. Only a structurally broken image
 * is an error.
 *
 * `out` is fully initialised on success and untouched on failure.
 *
 * @return ATLAS_OK; ATLAS_EFORMAT if the image is not ISO9660 or uses a
 *         block size this console cannot boot; ATLAS_EIO if `read`
 *         failed; ATLAS_EINVAL for a bad argument.
 */
atlas_err_t atlas_disc_probe(atlas_disc_read_fn read, void *ctx,
                             atlas_disc_info_t *out);

/**
 * Normalise a raw boot-path filename into a game ID:
 * "SLUS_209.02;1" becomes "SLUS-20902".
 *
 * Accepts the value with or without a "cdrom0:\" prefix, with either
 * slash, and with or without the ";1" version suffix - all four spellings
 * occur on real discs. Letters are upper-cased.
 *
 * Refuses rather than truncates: a shortened ID names a different game,
 * and this value is what a compatibility database is keyed on.
 *
 * @return ATLAS_OK, or ATLAS_EINVAL for a bad argument, a result that
 *         does not fit, or input with no recognisable ID in it.
 */
atlas_err_t atlas_disc_id_normalize(const char *raw, char *out, int size);

/**
 * The region a normalised game ID belongs to, from its four-letter
 * prefix. ATLAS_REGION_UNKNOWN for anything unrecognised, which is the
 * honest answer for homebrew and for prototypes.
 */
atlas_region_t atlas_disc_region_of_id(const char *id);

/** Short display name: "NTSC-U", "NTSC-J", "PAL", "Unknown". */
const char *atlas_disc_region_str(atlas_region_t region);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_DISC_H */
