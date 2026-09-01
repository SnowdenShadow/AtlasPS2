/*
 * AtlasPS2 - atlascdvd.h
 *
 * What the EE hands the IOP module at load time.
 *
 * The module is loaded with an argument block rather than reading a
 * config file, because at the point it runs there is no filesystem: the
 * IOP has just been reset, the game's modules are about to be loaded,
 * and everything the module will ever need to know has to already be in
 * the block it was started with.
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
#define ATLASCDVD_ARG_VERSION 1

#define ATLASCDVD_MAGIC 0x41544C43      /* 'ATLC' */

/* Must match ATLAS_FRAG_MAX in include/atlas/frag.h. The two are
 * checked against each other at compile time on the EE side. */
#define ATLASCDVD_FRAG_MAX 512

/* Bits in `flags`, filled from COMPAT.INI on the EE side. */
#define ATLASCDVD_F_FORCE_DVD   (1u << 0)   /* report PS2DVD regardless */
#define ATLASCDVD_F_HIDE_TRAY   (1u << 1)   /* never report a tray change */
#define ATLASCDVD_F_SLOW_FIRST  (1u << 2)   /* delay the first read */

/* Which device the extents are on. The module needs this because a
 * sector number means nothing without the device it is a sector of. */
#define ATLASCDVD_DEV_BDM       0           /* USB / any bdm block device */

typedef struct {
    unsigned int start;
    unsigned int count;
} atlascdvd_frag_t;

typedef struct {
    unsigned int magic;
    unsigned int version;

    unsigned int device;        /* ATLASCDVD_DEV_*                     */
    unsigned int device_index;  /* which one, when there are several   */

    unsigned int sector_size;   /* device sector size, in bytes        */
    unsigned int size_lo;       /* image length in bytes, low 32       */
    unsigned int size_hi;       /* ... and high 32: a dual-layer DVD
                                 * is 8.5 GB and does not fit in 32    */

    unsigned int flags;
    unsigned int layer1_lba;    /* 0 for a single-layer image          */

    unsigned int frag_count;
    atlascdvd_frag_t frag[ATLASCDVD_FRAG_MAX];
} atlascdvd_arg_t;

#endif /* ATLASCDVD_H */
