/*
 * AtlasPS2 - atlascdvd.h
 *
 * What the EE hands the IOP module at load time.
 *
 * The module is started with an argument block rather than reading a
 * config file, because at the point it runs there is no EE program left
 * to ask: the IOP has just been reset, the game's modules are about to
 * be loaded, and everything the module will ever need to know has to
 * already be in the block it was started with.
 *
 * WHY A PATH AND NOT A LIST OF SECTORS
 * ------------------------------------
 * An earlier version of this header carried the file's extent list -
 * 512 runs, 4 KB - built on the EE and copied across. It does not, now,
 * because the FAT reader in src/disc/frag.c compiles for the IOP
 * unchanged. The module walks the filesystem itself, once, in _start,
 * before the game exists.
 *
 * That is strictly better in the way that matters here: there is one
 * implementation of the arithmetic that decides which sectors a game
 * reads, and it is the one tests/test_frag.c is run against on every
 * build. Two copies of that arithmetic - one checked, one transcribed
 * into a module that cannot be tested - is precisely the arrangement
 * that produces a game reading somebody else's data with nothing to
 * show it did.
 *
 * The walk is still a walk of a live filesystem, but it happens before
 * a single line of the game has run, on an IOP that is doing nothing
 * else. Everything after it is raw sector reads.
 *
 * This header is shared verbatim between the EE side that fills the
 * block in and the IOP side that reads it. It is the one file in the
 * project compiled by both toolchains, which is why it uses only fixed
 * width types and no SDK headers at all.
 */
#ifndef ATLASCDVD_H
#define ATLASCDVD_H

/* Bumped whenever the layout below changes. The module refuses a block
 * carrying a different number rather than reading a field that has
 * moved - an argument block silently misread is a drive emulation
 * answering with somebody else's numbers. */
#define ATLASCDVD_ARG_VERSION 3

#define ATLASCDVD_MAGIC 0x41544C43      /* 'ATLC' */

/* Bits in `flags`, filled from COMPAT.INI on the EE side. */
#define ATLASCDVD_F_FORCE_DVD   (1u << 0)   /* report PS2DVD regardless */
#define ATLASCDVD_F_HIDE_TRAY   (1u << 1)   /* never report a tray change */
#define ATLASCDVD_F_SLOW_FIRST  (1u << 2)   /* delay the first read */

/* Which device the image is on. The module needs this because a path
 * means nothing without the device it is a path on. */
#define ATLASCDVD_DEV_BDM       0           /* USB / any bdm block device */
#define ATLASCDVD_DEV_HDD       1           /* HDL partition, raw ATA     */

/* Long enough for the paths a user actually types into a launcher, and
 * short enough that the whole block stays small: it is copied out of EE
 * memory the game is about to be given. */
#define ATLASCDVD_PATH_MAX      256

typedef struct {
    unsigned int magic;
    unsigned int version;

    unsigned int device;        /* ATLASCDVD_DEV_*                     */
    unsigned int device_index;  /* which one, when there are several   */

    unsigned int flags;
    unsigned int layer1_lba;    /* 0 for a single-layer image          */

    /* Valid only when device == ATLASCDVD_DEV_HDD: the absolute ATA
     * sector where the ISO data begins and its length in 512-byte ATA
     * sectors. The partition is one contiguous run, already known from
     * the EE side's own ioctl2 call, so there is nothing here to walk -
     * unlike ATLASCDVD_DEV_BDM's path, which _start() still resolves to
     * sectors itself. */
    unsigned int hdd_start_lba;
    unsigned int hdd_total_sectors;

    /* Absolute within the volume, either separator, no device prefix:
     * "/DVD/GAME.ISO". Valid only when device == ATLASCDVD_DEV_BDM; a
     * path carrying "mass:" would be a second, disagreeing answer to
     * the same question. */
    char path[ATLASCDVD_PATH_MAX];
} atlascdvd_arg_t;

#endif /* ATLASCDVD_H */
