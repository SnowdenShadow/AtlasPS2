/*
 * AtlasPS2 - sector.h
 *
 * Turning the 2048 bytes an image holds into the 2328, 2340 or 2352 a
 * game sometimes asks for.
 *
 * WHY THE OTHER SIZES EXIST
 * -------------------------
 * A CD sector on the physical disc is 2352 bytes: a 12-byte sync
 * pattern, a 4-byte header that says where the sector is, an 8-byte
 * subheader, then 2048 bytes of data, then error correction. A DVD
 * sector has no such framing at all - it is 2048 bytes and nothing
 * else.
 *
 * An image file stores only the 2048. Everything else is derivable
 * from the sector number, which is precisely why it is safe to throw
 * away and precisely why it must be regenerated exactly: a game that
 * asks for 2352 bytes and gets its own data at the wrong offset does
 * not fail, it reads sixteen bytes of header as the start of a
 * structure.
 *
 * WHO ASKS FOR THEM
 * -----------------
 * Most titles read 2048 and never touch this. The ones that do not are
 * usually reading Mode 2 Form 2 sectors - streamed audio and video,
 * where the subheader carries the channel number the title is
 * interleaving on. Handing those back with a zeroed subheader gives a
 * game that plays every channel at once, or none.
 *
 * WHAT THIS DELIBERATELY DOES NOT DO
 * ----------------------------------
 * The 276 bytes of error correction in a Mode 1 sector are computed
 * from the data with a Reed-Solomon product code. They are *not*
 * computed here: they are filled with zeroes, because nothing in a PS2
 * game checks them - the drive corrects errors before the data reaches
 * the IOP, so by the time a title sees a sector the ECC has already
 * done its job and is dead weight. Computing it would cost more time
 * per sector than the read itself.
 *
 * This is the one place in this module where the answer is knowingly
 * not what a real drive would return. It is written down here rather
 * than discovered later.
 */
#ifndef ATLAS_SECTOR_H
#define ATLAS_SECTOR_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The sizes sceCdRead() can be asked for, in the order the CDVD
 * datapattern field numbers them. The names match the SDK's
 * SCECdSecS2048/2328/2340 so that a reader can check them against
 * libcdvd-common.h without translating.
 */
#define ATLAS_SECTOR_2048   0
#define ATLAS_SECTOR_2328   1
#define ATLAS_SECTOR_2340   2
#define ATLAS_SECTOR_2352   3   /* not a datapattern value; see below */

/**
 * How many bytes one sector occupies in the given mode.
 *
 * @return 2048, 2328, 2340, 2352, or 0 for a mode this does not know -
 *         which the caller must treat as a failed read rather than
 *         substituting 2048, since a game that asked for 2340 and got
 *         2048 reads the next sector's data as the tail of this one.
 */
u32 atlas_sector_size(int mode);

/**
 * Expand one 2048-byte sector into `mode`'s layout.
 *
 * @param lba   the sector's own number, which is what the header and
 *              subheader are derived from
 * @param form2 non-zero if this sector should be presented as Mode 2
 *              Form 2 (streamed media) rather than Mode 2 Form 1
 * @param data  the 2048 bytes from the image
 * @param out   receives atlas_sector_size(mode) bytes
 *
 * @return ATLAS_OK, or ATLAS_EINVAL for an unknown mode or a NULL
 *         argument.
 */
atlas_err_t atlas_sector_expand(int mode, u32 lba, int form2,
                                const void *data, void *out);

/**
 * The 4-byte CD header for `lba`: minute, second, frame, mode.
 *
 * Exposed because it is the part with an off-by-one in it. The address
 * is BCD, counted from a 2-second pre-gap, at 75 frames per second -
 * three conversions, each of which is wrong in a way that still returns
 * a plausible number.
 */
void atlas_sector_header(u32 lba, unsigned char out[4]);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_SECTOR_H */
