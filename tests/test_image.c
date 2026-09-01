/*
 * AtlasPS2 - test_image.c
 *
 * The layer that turns a file into sectors.
 *
 * THE CENTRAL CHECK
 * -----------------
 * The generator writes the same disc twice: once as a plain ISO, once
 * as a ZSO. Every sector of the two must be identical. That comparison
 * is what makes this check worth having - a ZSO index bug does not
 * produce garbage, it produces the *wrong block*, which decodes
 * perfectly and returns 2048 plausible bytes from elsewhere on the
 * disc. Against a fixed expected value that passes; against the same
 * disc in another format it cannot.
 *
 * The images are built by tools/genimage.py rather than committed, and
 * the check skips rather than fails if they are absent: the ZSO writer
 * needs Python's lz4 module, and a contributor without it should still
 * be able to run every other self-check.
 *
 * WHAT THIS CANNOT COVER
 * ----------------------
 * Throughput. Whether a block decodes fast enough to keep a game's
 * streaming audio fed is a property of a 37 MHz IOP, and no answer
 * obtained here transfers to that. It is measured on hardware.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "atlas/image.h"
#include "atlas/disc.h"

#define IMG_DIR "../build/testimg/"

#define SECTOR ATLAS_DISC_SECTOR_SIZE

static int have(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;

    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* ISO and ZSO must be the same disc                                   */
/* ------------------------------------------------------------------ */

static void check_formats_agree(const char *zso_path)
{
    atlas_image_t *iso = NULL, *zso = NULL;
    unsigned char a[SECTOR * 4], b[SECTOR * 4];
    u32 total, lba;

    assert(atlas_image_open(IMG_DIR "test.iso", &iso) == ATLAS_OK);
    assert(atlas_image_open(zso_path, &zso) == ATLAS_OK);

    assert(atlas_image_format(iso) == ATLAS_IMAGE_ISO);
    assert(atlas_image_format(zso) == ATLAS_IMAGE_ZSO);

    total = atlas_image_sectors(iso);
    assert(total > 16);
    assert(atlas_image_sectors(zso) == total);

    /*
     * Every sector, one at a time. A wrong index entry returns a
     * different sector of the same disc - real data, correctly
     * decompressed, from the wrong place - and only a comparison
     * against the uncompressed original catches that.
     */
    for (lba = 0; lba < total; lba++) {
        assert(atlas_image_read(iso, lba, 1, a) == 0);

        if (atlas_image_read(zso, lba, 1, b) != 0) {
            printf("test_image: %s sector %u unreadable\n", zso_path, lba);
            assert(0);
        }

        if (memcmp(a, b, SECTOR) != 0) {
            printf("test_image: %s sector %u differs from the ISO\n",
                   zso_path, lba);
            assert(0);
        }
    }

    /*
     * Multi-sector reads, which is how a directory extent is actually
     * fetched. Four sectors is more than one 2 KB block, so this
     * crosses a block boundary - the case where an off-by-one in the
     * block arithmetic shows up and single-sector reads do not.
     */
    for (lba = 0; lba + 4 <= total; lba += 3) {
        assert(atlas_image_read(iso, lba, 4, a) == 0);
        assert(atlas_image_read(zso, lba, 4, b) == 0);
        assert(memcmp(a, b, SECTOR * 4) == 0);
    }

    /*
     * Backwards, to defeat the cache. Reading forwards hits the cached
     * block almost every time, so a cache that returned stale data
     * would pass the loops above; going backwards misses on every
     * sector and forces a real load.
     */
    for (lba = total; lba-- > 0; ) {
        assert(atlas_image_read(iso, lba, 1, a) == 0);
        assert(atlas_image_read(zso, lba, 1, b) == 0);
        assert(memcmp(a, b, SECTOR) == 0);
    }

    atlas_image_close(iso);
    atlas_image_close(zso);
}

/* ------------------------------------------------------------------ */
/* Bounds                                                              */
/* ------------------------------------------------------------------ */

static void check_bounds(void)
{
    atlas_image_t *img = NULL;
    unsigned char buf[SECTOR * 2];
    u32 total;

    assert(atlas_image_open(IMG_DIR "test.iso", &img) == ATLAS_OK);
    total = atlas_image_sectors(img);

    /* The last sector is readable; one past it is not. */
    assert(atlas_image_read(img, total - 1, 1, buf) == 0);
    assert(atlas_image_read(img, total, 1, buf) != 0);

    /* A read that starts inside and ends outside must fail whole. A
     * partial success would leave the tail of the buffer holding
     * whatever was there before, and the caller cannot tell. */
    assert(atlas_image_read(img, total - 1, 2, buf) != 0);

    /*
     * An LBA near the top of the range, where lba + count wraps. Without
     * the overflow check the comparison against the sector count passes
     * and the seek goes somewhere arbitrary.
     */
    assert(atlas_image_read(img, 0xFFFFFFFFU, 2, buf) != 0);

    assert(atlas_image_read(img, 0, 0, buf) != 0);
    assert(atlas_image_read(NULL, 0, 1, buf) != 0);
    assert(atlas_image_read(img, 0, 1, NULL) != 0);

    atlas_image_close(img);
}

/* ------------------------------------------------------------------ */
/* Identification through the image layer                              */
/* ------------------------------------------------------------------ */

static void check_probe_through_image(void)
{
    const char *paths[] = {
        IMG_DIR "test.iso",
        IMG_DIR "test.zso",
        IMG_DIR "test16.zso"
    };
    int i;

    /*
     * The two layers joined up: atlas_image_read matches the signature
     * disc.c asks for, so an open image goes straight into the prober.
     * All three files are the same disc, so all three must identify
     * identically - a ZSO whose directory sector decoded wrongly would
     * report a different game, or none.
     */
    for (i = 0; i < 3; i++) {
        atlas_image_t *img = NULL;
        atlas_disc_info_t info;

        if (!have(paths[i]))
            continue;

        assert(atlas_image_open(paths[i], &img) == ATLAS_OK);
        assert(atlas_disc_probe(atlas_image_read, img, &info) == ATLAS_OK);

        assert(strcmp(info.id, "SLUS-20902") == 0);
        assert(strcmp(info.boot, "cdrom0:\\SLUS_209.02;1") == 0);
        assert(strcmp(info.volume, "ATLAS_TEST_DISC") == 0);
        assert(info.region == ATLAS_REGION_NTSC_U);

        atlas_image_close(img);
    }
}

/* ------------------------------------------------------------------ */
/* Rejection                                                           */
/* ------------------------------------------------------------------ */

static void check_rejects(void)
{
    atlas_image_t *img = NULL;
    FILE *f;

    assert(atlas_image_open(IMG_DIR "nothing-here.iso", &img) == ATLAS_ENOENT);
    assert(img == NULL);

    assert(atlas_image_open(NULL, &img) == ATLAS_EINVAL);
    assert(atlas_image_open("", &img) == ATLAS_EINVAL);
    assert(atlas_image_open(IMG_DIR "test.iso", NULL) == ATLAS_EINVAL);

    /*
     * A file too short to hold a volume descriptor set. Accepting it
     * would report a truncated download as a disc whose directory
     * merely failed to read, and send the user hunting for a bad dump.
     */
    f = fopen(IMG_DIR "tiny.iso", "wb");
    if (f) {
        static const char zeros[SECTOR] = { 0 };
        fwrite(zeros, 1, sizeof(zeros), f);
        fclose(f);

        assert(atlas_image_open(IMG_DIR "tiny.iso", &img) == ATLAS_EFORMAT);
        remove(IMG_DIR "tiny.iso");
    }

    /*
     * A ZSO whose header claims an impossible block size. Believing it
     * would size the decompression buffer from a number the file chose.
     */
    f = fopen(IMG_DIR "bad.zso", "wb");
    if (f) {
        unsigned char hdr[64];

        memset(hdr, 0, sizeof(hdr));
        memcpy(hdr, "ZISO", 4);
        hdr[4] = 24;
        hdr[8] = 0x00; hdr[9] = 0x00; hdr[10] = 0x01;  /* 64 KB total */
        hdr[16] = 0x11; hdr[17] = 0x00;                /* block 17 - odd */
        hdr[20] = 1;

        fwrite(hdr, 1, sizeof(hdr), f);
        fclose(f);

        assert(atlas_image_open(IMG_DIR "bad.zso", &img) == ATLAS_EFORMAT);
        remove(IMG_DIR "bad.zso");
    }
}

int main(void)
{
    if (!have(IMG_DIR "test.iso") || !have(IMG_DIR "test.zso")) {
        /*
         * Skipping rather than failing. Building the ZSO needs Python's
         * lz4 module, and a contributor without it should still get a
         * useful run out of every other check.
         */
        printf("test_image: SKIPPED (run tools/genimage.py first)\n");
        return 0;
    }

    check_formats_agree(IMG_DIR "test.zso");

    if (have(IMG_DIR "test16.zso"))
        check_formats_agree(IMG_DIR "test16.zso");

    check_bounds();
    check_probe_through_image();
    check_rejects();

    printf("test_image: OK\n");
    return 0;
}
