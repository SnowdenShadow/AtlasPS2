/*
 * AtlasPS2 - sector.c
 * Synthesising CD sector framing around the 2048 bytes an image holds.
 *
 * Pure arithmetic over bytes, which is the only reason this part of the
 * drive emulation can be checked on a build machine at all. It is also
 * the part where being wrong is invisible: every layout below returns
 * the right number of bytes whether or not the fields inside them mean
 * anything.
 */
#include <string.h>

#include "atlas/sector.h"

/*
 * A CD address is counted from two seconds before the first sector -
 * the pre-gap, which is not stored on the disc but is part of the
 * numbering. 75 sectors make a second, 60 seconds a minute.
 */
#define FRAMES_PER_SECOND   75
#define PREGAP_SECONDS      2

/* The sync pattern that opens every physical CD sector: one zero byte,
 * ten 0xFF, one zero. A game looking for a sector boundary looks for
 * exactly this. */
static const unsigned char k_sync[12] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

static unsigned char to_bcd(u32 v)
{
    return (unsigned char)(((v / 10) % 10) * 16 + (v % 10));
}

u32 atlas_sector_size(int mode)
{
    switch (mode) {
    case ATLAS_SECTOR_2048: return 2048;
    case ATLAS_SECTOR_2328: return 2328;
    case ATLAS_SECTOR_2340: return 2340;
    case ATLAS_SECTOR_2352: return 2352;
    default:                return 0;
    }
}

void atlas_sector_header(u32 lba, unsigned char out[4])
{
    u32 total = lba + PREGAP_SECONDS * FRAMES_PER_SECOND;

    out[0] = to_bcd(total / (FRAMES_PER_SECOND * 60));
    out[1] = to_bcd((total / FRAMES_PER_SECOND) % 60);
    out[2] = to_bcd(total % FRAMES_PER_SECOND);
    out[3] = 2;         /* Mode 2, which is what a PS2 disc is */
}

/**
 * The 8-byte subheader, which is two identical 4-byte copies.
 *
 * file 0, channel 0, and a submode byte that says only "data" or
 * "form 2 data" plus end-of-record. A real disc carries the title's own
 * interleaving in these; an image does not record them, so the neutral
 * values are what is returned. A title that interleaves channels reads
 * its own data through this and will need its subheaders preserved -
 * which is a change to the image format, not to this function, and is
 * why the form2 flag is threaded through rather than assumed.
 */
static void subheader(int form2, unsigned char out[8])
{
    out[0] = 0;                                 /* file           */
    out[1] = 0;                                 /* channel        */
    out[2] = (unsigned char)(form2 ? 0x28 : 0x08);  /* submode    */
    out[3] = 0;                                 /* coding info    */

    out[4] = out[0];
    out[5] = out[1];
    out[6] = out[2];
    out[7] = out[3];
}

atlas_err_t atlas_sector_expand(int mode, u32 lba, int form2,
                                const void *data, void *out)
{
    unsigned char *p = (unsigned char *)out;

    if (!data || !out)
        return ATLAS_EINVAL;

    switch (mode) {
    case ATLAS_SECTOR_2048:
        memcpy(p, data, 2048);
        return ATLAS_OK;

    case ATLAS_SECTOR_2328:
        /*
         * Form 2 user data: 2324 bytes plus a 4-byte EDC. The 2048 an
         * image holds is less than that, so the remainder is zero -
         * which is what it is on a disc whose sectors were mastered as
         * Form 1 anyway. The size is what the game asked for and the
         * data is at offset 0, which is where it looks.
         */
        memcpy(p, data, 2048);
        memset(p + 2048, 0, 2328 - 2048);
        return ATLAS_OK;

    case ATLAS_SECTOR_2340:
        /* Everything but the 12-byte sync: header, subheader, data. */
        atlas_sector_header(lba, p);
        subheader(form2, p + 4);
        memcpy(p + 12, data, 2048);
        memset(p + 12 + 2048, 0, 2340 - 12 - 2048);
        return ATLAS_OK;

    case ATLAS_SECTOR_2352:
        memcpy(p, k_sync, sizeof(k_sync));
        atlas_sector_header(lba, p + 12);
        subheader(form2, p + 16);
        memcpy(p + 24, data, 2048);
        /* EDC and ECC. Deliberately zero - see sector.h. */
        memset(p + 24 + 2048, 0, 2352 - 24 - 2048);
        return ATLAS_OK;

    default:
        /*
         * Not substituting 2048: a caller that asked for 2340 and was
         * given 2048 bytes reads the next sector's data as the tail of
         * this one, with nothing to say it happened.
         */
        return ATLAS_EINVAL;
    }
}
