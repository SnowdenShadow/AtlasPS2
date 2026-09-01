/*
 * AtlasPS2 - test_sector.c
 *
 * The sector framing synthesised around an image's 2048 bytes.
 *
 * Every layout here returns the right *number* of bytes whether or not
 * the fields inside it mean anything, so a check that only measured
 * lengths would pass a completely wrong implementation. What is checked
 * instead is where the user data lands, that the MSF address round-trips
 * back to the sector number it came from, and that the BCD is BCD rather
 * than a number that merely looks like one below 10.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/sector.h"

static unsigned char g_data[2048];
static unsigned char g_out[4096];

static void fill_data(unsigned seed)
{
    int i;

    for (i = 0; i < 2048; i++)
        g_data[i] = (unsigned char)(seed + i * 31);
}

/** Read a BCD byte back as an ordinary number, or -1 if it is not BCD. */
static int from_bcd(unsigned char b)
{
    if ((b & 0x0F) > 9 || (b >> 4) > 9)
        return -1;

    return (b >> 4) * 10 + (b & 0x0F);
}

static void check_sizes(void)
{
    assert(atlas_sector_size(ATLAS_SECTOR_2048) == 2048);
    assert(atlas_sector_size(ATLAS_SECTOR_2328) == 2328);
    assert(atlas_sector_size(ATLAS_SECTOR_2340) == 2340);
    assert(atlas_sector_size(ATLAS_SECTOR_2352) == 2352);

    /* An unknown mode reports 0, and expand refuses it. A caller given
     * 2048 for a mode it asked 2340 for reads the next sector's data as
     * the tail of this one. */
    assert(atlas_sector_size(4) == 0);
    assert(atlas_sector_size(-1) == 0);
    assert(atlas_sector_size(99) == 0);

    assert(atlas_sector_expand(4, 0, 0, g_data, g_out) == ATLAS_EINVAL);
    assert(atlas_sector_expand(-1, 0, 0, g_data, g_out) == ATLAS_EINVAL);
    assert(atlas_sector_expand(ATLAS_SECTOR_2048, 0, 0, NULL, g_out)
           == ATLAS_EINVAL);
    assert(atlas_sector_expand(ATLAS_SECTOR_2048, 0, 0, g_data, NULL)
           == ATLAS_EINVAL);
}

/**
 * The address must survive the round trip.
 *
 * BCD, a 2-second pre-gap and 75 frames to the second are three
 * conversions, and each one is wrong in a way that still produces a
 * plausible three-byte number. Converting back is the only check that
 * catches all three.
 */
static void check_header(void)
{
    static const u32 k_lba[] = {
        0, 1, 74, 75, 76, 149, 150, 151,
        4499, 4500, 4501,               /* the minute boundary */
        16, 12345, 65535, 100000, 333000
    };
    u32 i;

    for (i = 0; i < sizeof(k_lba) / sizeof(k_lba[0]); i++) {
        unsigned char h[4];
        int m, s, f;
        u32 back;

        atlas_sector_header(k_lba[i], h);

        m = from_bcd(h[0]);
        s = from_bcd(h[1]);
        f = from_bcd(h[2]);

        assert(m >= 0 && s >= 0 && f >= 0);     /* really BCD */
        assert(s < 60);
        assert(f < 75);
        assert(h[3] == 2);                      /* Mode 2 */

        back = (u32)m * 60 * 75 + (u32)s * 75 + (u32)f;
        assert(back == k_lba[i] + 150);         /* the 2-second pre-gap */
    }

    /*
     * BCD below 10 looks identical to plain binary, so a header built
     * without the conversion passes every small case. LBA 0 is
     * 00:02:00; LBA 4350 is 01:00:00, where the minute field is 1 in
     * both encodings but the seconds are not.
     */
    {
        unsigned char h[4];

        atlas_sector_header(0, h);
        assert(h[0] == 0x00 && h[1] == 0x02 && h[2] == 0x00);

        /* 150 + 4350 = 4500 = exactly one minute. */
        atlas_sector_header(4350, h);
        assert(h[0] == 0x01 && h[1] == 0x00 && h[2] == 0x00);

        /* 150 + 74 = 224 = 0:02:74, the last frame before 3 seconds -
         * where BCD and binary differ (0x74 vs 74). */
        atlas_sector_header(74, h);
        assert(h[0] == 0x00 && h[1] == 0x02 && h[2] == 0x74);

        /* 150 + 75 = 225 = 0:03:00. */
        atlas_sector_header(75, h);
        assert(h[1] == 0x03 && h[2] == 0x00);
    }
}

/** The user data must land where the mode says, byte for byte. */
static void check_layouts(void)
{
    fill_data(0x5A);

    /* 2048: the bytes and nothing else. */
    memset(g_out, 0xCC, sizeof(g_out));
    assert(atlas_sector_expand(ATLAS_SECTOR_2048, 16, 0, g_data, g_out)
           == ATLAS_OK);
    assert(memcmp(g_out, g_data, 2048) == 0);
    assert(g_out[2048] == 0xCC);        /* nothing written past the end */

    /* 2328: data at offset 0, the rest cleared. */
    memset(g_out, 0xCC, sizeof(g_out));
    assert(atlas_sector_expand(ATLAS_SECTOR_2328, 16, 0, g_data, g_out)
           == ATLAS_OK);
    assert(memcmp(g_out, g_data, 2048) == 0);
    {
        int i;
        for (i = 2048; i < 2328; i++)
            assert(g_out[i] == 0x00);
    }
    assert(g_out[2328] == 0xCC);

    /* 2340: header, subheader, then data at offset 12. */
    memset(g_out, 0xCC, sizeof(g_out));
    assert(atlas_sector_expand(ATLAS_SECTOR_2340, 16, 0, g_data, g_out)
           == ATLAS_OK);
    {
        unsigned char h[4];

        atlas_sector_header(16, h);
        assert(memcmp(g_out, h, 4) == 0);
    }
    assert(memcmp(g_out + 12, g_data, 2048) == 0);
    assert(g_out[2340] == 0xCC);

    /* 2352: sync, header, subheader, then data at offset 24. */
    memset(g_out, 0xCC, sizeof(g_out));
    assert(atlas_sector_expand(ATLAS_SECTOR_2352, 16, 0, g_data, g_out)
           == ATLAS_OK);
    {
        static const unsigned char k_sync[12] = {
            0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
        };
        unsigned char h[4];

        assert(memcmp(g_out, k_sync, 12) == 0);

        atlas_sector_header(16, h);
        assert(memcmp(g_out + 12, h, 4) == 0);
    }
    assert(memcmp(g_out + 24, g_data, 2048) == 0);
    assert(g_out[2352] == 0xCC);

    /* The two large modes carry the same header for the same LBA -
     * they differ only by the sync in front of it. */
    {
        unsigned char a[2340], b[2352];

        assert(atlas_sector_expand(ATLAS_SECTOR_2340, 12345, 0, g_data, a)
               == ATLAS_OK);
        assert(atlas_sector_expand(ATLAS_SECTOR_2352, 12345, 0, g_data, b)
               == ATLAS_OK);
        /* Header, subheader and data all sit 12 bytes later in the
         * 2352 form; everything up to the end of the data must match. */
        assert(memcmp(a, b + 12, 12 + 2048) == 0);
    }
}

/**
 * The subheader is two identical copies, and the form-2 bit is the one
 * field in it that is not a constant.
 *
 * A title streaming audio reads its channel out of here. A form-2
 * sector handed back marked form 1 is 2048 bytes of media presented as
 * though it were file data.
 */
static void check_subheader(void)
{
    unsigned char f1[2352], f2[2352];

    fill_data(0x11);

    assert(atlas_sector_expand(ATLAS_SECTOR_2352, 1000, 0, g_data, f1)
           == ATLAS_OK);
    assert(atlas_sector_expand(ATLAS_SECTOR_2352, 1000, 1, g_data, f2)
           == ATLAS_OK);

    /* Two copies, in both forms. */
    assert(memcmp(f1 + 16, f1 + 20, 4) == 0);
    assert(memcmp(f2 + 16, f2 + 20, 4) == 0);

    /* File and channel are zero; the submode carries the form. */
    assert(f1[16] == 0 && f1[17] == 0);
    assert(f2[16] == 0 && f2[17] == 0);

    assert((f1[18] & 0x20) == 0);       /* form 1 */
    assert((f2[18] & 0x20) != 0);       /* form 2 */
    assert((f1[18] & 0x08) != 0);       /* data */
    assert((f2[18] & 0x08) != 0);

    /* The flag changes the subheader and nothing else. */
    assert(memcmp(f1, f2, 16) == 0);
    assert(memcmp(f1 + 24, f2 + 24, 2352 - 24) == 0);

    /* 2340 carries the same subheader, 12 bytes earlier. */
    {
        unsigned char s[2340];

        assert(atlas_sector_expand(ATLAS_SECTOR_2340, 1000, 1, g_data, s)
               == ATLAS_OK);
        assert(memcmp(s + 4, f2 + 16, 8) == 0);
    }
}

int main(void)
{
    check_sizes();
    check_header();
    check_layouts();
    check_subheader();

    printf("test_sector: OK\n");
    return 0;
}
