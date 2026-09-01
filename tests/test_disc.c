/*
 * AtlasPS2 - test_disc.c
 *
 * The parser that decides what a disc is.
 *
 * Everything downstream - the row the user sees, the compatibility
 * entry that gets applied, the video mode the emulated drive reports -
 * is keyed on the game ID this file extracts. A wrong ID does not fail
 * loudly: it matches another game's compatibility entry and produces a
 * console that hangs on a black screen for reasons the user cannot see.
 *
 * So the checks here build ISO9660 images byte by byte rather than
 * loading a fixture. A fixture would only prove the parser agrees with
 * whatever mastering tool produced it; a constructed image lets each
 * check state the exact malformation it is about - a bare-CR file, a
 * directory record claiming a length past the end of its sector, a
 * BOOT2 longer than the field that holds it.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "atlas/disc.h"

/* ------------------------------------------------------------------ */
/* A synthetic disc                                                    */
/*                                                                     */
/* Big enough for the descriptor set (16-32), a root directory and a   */
/* SYSTEM.CNF, with room to spare for the malformed layouts below.     */
/* ------------------------------------------------------------------ */

#define IMG_SECTORS 40
#define SECSZ       ATLAS_DISC_SECTOR_SIZE

#define LBA_PVD     16
#define LBA_ROOT    20
#define LBA_CNF     22

typedef struct {
    unsigned char data[IMG_SECTORS * SECSZ];
    int fail_at;    /**< LBA that returns an I/O error, or -1 */
    int reads;
} img_t;

static int img_read(void *ctx, u32 lba, u32 count, void *buf)
{
    img_t *m = (img_t *)ctx;

    m->reads++;

    if (m->fail_at >= 0 && (u32)m->fail_at == lba)
        return -1;

    /* A read past the end is a failure, never zeroes: zeroes parse as a
     * valid empty directory, which would hide the fault. */
    if (lba + count > IMG_SECTORS)
        return -1;

    memcpy(buf, m->data + (size_t)lba * SECSZ, (size_t)count * SECSZ);
    return 0;
}

static void put32(unsigned char *p, u32 v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static void put16(unsigned char *p, u16 v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

/** Write a directory record for a file at `off` in a sector. */
static int put_record(unsigned char *at, const char *name,
                      u32 lba, u32 size, int is_dir)
{
    int name_len = (int)strlen(name);
    int len = 33 + name_len;

    /* Records are padded to an even length, as the standard requires. */
    if (len & 1)
        len++;

    memset(at, 0, (size_t)len);
    at[0] = (unsigned char)len;
    put32(at + 2, lba);
    put32(at + 10, size);
    at[25] = (unsigned char)(is_dir ? 0x02 : 0x00);
    at[32] = (unsigned char)name_len;
    memcpy(at + 33, name, (size_t)name_len);

    return len;
}

/**
 * Build a valid image with the given SYSTEM.CNF contents.
 *
 * `cnf` may be NULL, which builds a disc whose root directory holds
 * something else entirely - a data disc, which must probe as a disc and
 * not as a failure.
 */
static void build(img_t *m, const char *volume, const char *cnf)
{
    unsigned char *pvd = m->data + (size_t)LBA_PVD * SECSZ;
    unsigned char *root = m->data + (size_t)LBA_ROOT * SECSZ;
    int off = 0;
    int cnf_len = cnf ? (int)strlen(cnf) : 0;

    memset(m, 0, sizeof(*m));
    m->fail_at = -1;

    pvd[0] = 1;                       /* primary volume descriptor */
    memcpy(pvd + 1, "CD001", 5);
    memset(pvd + 40, ' ', 32);        /* volume id, space-padded   */
    memcpy(pvd + 40, volume, strlen(volume));
    put32(pvd + 80, IMG_SECTORS);
    put16(pvd + 128, SECSZ);
    put_record(pvd + 156, "\0", LBA_ROOT, SECSZ, 1);

    /* The terminator, so a scan that misses the PVD still ends. */
    m->data[(size_t)(LBA_PVD + 1) * SECSZ] = 255;
    memcpy(m->data + (size_t)(LBA_PVD + 1) * SECSZ + 1, "CD001", 5);

    /* "." and ".." come first on every real disc; including them means
     * the walk is tested against the layout it will actually meet. */
    off += put_record(root + off, "\0", LBA_ROOT, SECSZ, 1);
    off += put_record(root + off, "\1", LBA_ROOT, SECSZ, 1);

    if (cnf) {
        off += put_record(root + off, "SYSTEM.CNF;1",
                          LBA_CNF, (u32)cnf_len, 0);
        memcpy(m->data + (size_t)LBA_CNF * SECSZ, cnf, (size_t)cnf_len);
    } else {
        off += put_record(root + off, "MOVIE.PSS;1", 30, 1234, 0);
    }
}

/* ------------------------------------------------------------------ */
/* ID normalisation                                                    */
/* ------------------------------------------------------------------ */

static void check_id_normalize(void)
{
    char id[ATLAS_DISC_ID_MAX];

    /* The four spellings that occur on real discs, all one game. */
    assert(atlas_disc_id_normalize("cdrom0:\\SLUS_209.02;1",
                                   id, sizeof(id)) == ATLAS_OK);
    assert(strcmp(id, "SLUS-20902") == 0);

    assert(atlas_disc_id_normalize("cdrom0:/SLUS_209.02;1",
                                   id, sizeof(id)) == ATLAS_OK);
    assert(strcmp(id, "SLUS-20902") == 0);

    assert(atlas_disc_id_normalize("cdrom0:SLUS_209.02",
                                   id, sizeof(id)) == ATLAS_OK);
    assert(strcmp(id, "SLUS-20902") == 0);

    assert(atlas_disc_id_normalize("SLUS_209.02;1",
                                   id, sizeof(id)) == ATLAS_OK);
    assert(strcmp(id, "SLUS-20902") == 0);

    /* Lower case happens; the ID is a database key and must not vary. */
    assert(atlas_disc_id_normalize("cdrom0:\\slus_209.02;1",
                                   id, sizeof(id)) == ATLAS_OK);
    assert(strcmp(id, "SLUS-20902") == 0);

    /* Already normalised input must survive a second pass unchanged:
     * config files store the normalised form and get re-read. */
    assert(atlas_disc_id_normalize("SLUS-20902", id, sizeof(id)) == ATLAS_OK);
    assert(strcmp(id, "SLUS-20902") == 0);

    /*
     * Things that are not IDs. Each of these would otherwise become a
     * database key, and a key that matches the wrong entry applies
     * another game's patches to this one.
     */
    assert(atlas_disc_id_normalize("cdrom0:\\BOOT.ELF;1",
                                   id, sizeof(id)) != ATLAS_OK);
    assert(atlas_disc_id_normalize("", id, sizeof(id)) != ATLAS_OK);
    assert(atlas_disc_id_normalize("cdrom0:\\", id, sizeof(id)) != ATLAS_OK);
    assert(atlas_disc_id_normalize("SLUS_", id, sizeof(id)) != ATLAS_OK);
    assert(atlas_disc_id_normalize("SLUS_1", id, sizeof(id)) != ATLAS_OK);
    assert(atlas_disc_id_normalize("SLU_209.02", id, sizeof(id)) != ATLAS_OK);
    assert(atlas_disc_id_normalize(NULL, id, sizeof(id)) == ATLAS_EINVAL);

    /*
     * Refusing beats truncating. "SLUS-20902" shortened to "SLUS-209"
     * is a well-formed ID belonging to a different game, so a caller
     * that ignored the return value would look up the wrong entry.
     */
    {
        char small[9];
        assert(atlas_disc_id_normalize("SLUS_209.02;1",
                                       small, sizeof(small)) == ATLAS_EINVAL);
    }
}

/* ------------------------------------------------------------------ */
/* Region                                                              */
/* ------------------------------------------------------------------ */

static void check_region(void)
{
    assert(atlas_disc_region_of_id("SLUS-20902") == ATLAS_REGION_NTSC_U);
    assert(atlas_disc_region_of_id("SCUS-97129") == ATLAS_REGION_NTSC_U);
    assert(atlas_disc_region_of_id("SLES-50490") == ATLAS_REGION_PAL);
    assert(atlas_disc_region_of_id("SCES-50916") == ATLAS_REGION_PAL);
    assert(atlas_disc_region_of_id("SLPS-25234") == ATLAS_REGION_NTSC_J);
    assert(atlas_disc_region_of_id("SLKA-25123") == ATLAS_REGION_NTSC_J);

    /* Homebrew and prototypes have no meaningful prefix. Guessing one
     * would set a video mode on the user's behalf and get it wrong. */
    assert(atlas_disc_region_of_id("ZZZZ-00000") == ATLAS_REGION_UNKNOWN);
    assert(atlas_disc_region_of_id("") == ATLAS_REGION_UNKNOWN);
    assert(atlas_disc_region_of_id(NULL) == ATLAS_REGION_UNKNOWN);

    /* Never NULL: this goes straight into a printf on the console. */
    assert(atlas_disc_region_str(ATLAS_REGION_PAL) != NULL);
    assert(atlas_disc_region_str((atlas_region_t)99) != NULL);
}

/* ------------------------------------------------------------------ */
/* Probing a whole image                                               */
/* ------------------------------------------------------------------ */

static void check_probe_basic(void)
{
    img_t m;
    atlas_disc_info_t info;

    build(&m, "SHADOW_OF_THE_COLOSSUS",
          "BOOT2 = cdrom0:\\SCUS_974.72;1\r\n"
          "VER = 1.00\r\n"
          "VMODE = NTSC\r\n");

    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(strcmp(info.id, "SCUS-97472") == 0);
    assert(strcmp(info.boot, "cdrom0:\\SCUS_974.72;1") == 0);
    assert(strcmp(info.volume, "SHADOW_OF_THE_COLOSSUS") == 0);
    assert(info.region == ATLAS_REGION_NTSC_U);
    assert(info.sectors == IMG_SECTORS);
}

static void check_line_endings(void)
{
    img_t m;
    atlas_disc_info_t info;
    const char *bodies[] = {
        "BOOT2 = cdrom0:\\SLES_504.90;1\nVER = 1.00\n",      /* LF   */
        "BOOT2 = cdrom0:\\SLES_504.90;1\r\nVER = 1.00\r\n",  /* CRLF */
        "BOOT2 = cdrom0:\\SLES_504.90;1\rVER = 1.00\r"       /* CR   */
    };
    int i;

    /*
     * The bare-CR case is the one that matters. Splitting only on LF
     * would make the whole file one line, and BOOT2's value would
     * swallow "VER = 1.00" - a boot path no disc contains, on a game
     * that would otherwise have launched.
     */
    for (i = 0; i < 3; i++) {
        build(&m, "GAME", bodies[i]);
        assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
        assert(strcmp(info.id, "SLES-50490") == 0);
        assert(strcmp(info.boot, "cdrom0:\\SLES_504.90;1") == 0);
        assert(info.region == ATLAS_REGION_PAL);
    }
}

static void check_vmode_overrides_prefix(void)
{
    img_t m;
    atlas_disc_info_t info;

    /*
     * A disc whose prefix says one thing and whose VMODE says another
     * is believed on VMODE: the discs that carry the key are largely
     * the ones mastered against their prefix. Getting this backwards
     * gives a game running a fifth too fast with the picture cut off.
     */
    build(&m, "GAME",
          "BOOT2 = cdrom0:\\SLUS_209.02;1\nVMODE = PAL\n");
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(strcmp(info.id, "SLUS-20902") == 0);
    assert(info.region == ATLAS_REGION_PAL);

    /* But VMODE=NTSC must not downgrade a known PAL prefix to NTSC-U:
     * a PAL disc saying NTSC means "not-PAL-60", not "American". */
    build(&m, "GAME",
          "BOOT2 = cdrom0:\\SLES_504.90;1\nVMODE = NTSC\n");
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(info.region == ATLAS_REGION_PAL);
}

static void check_data_disc(void)
{
    img_t m;
    atlas_disc_info_t info;

    /*
     * No SYSTEM.CNF is a disc, not a failure. Reporting it as an error
     * would hide the row entirely; reporting it with an empty ID lets
     * the browser show it and refuse to launch it, which is what the
     * user needs to see.
     */
    build(&m, "DATA_DISC", NULL);
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(info.id[0] == '\0');
    assert(info.boot[0] == '\0');
    assert(strcmp(info.volume, "DATA_DISC") == 0);
    assert(info.region == ATLAS_REGION_UNKNOWN);
}

static void check_not_iso(void)
{
    img_t m;
    atlas_disc_info_t info;

    /* All zeroes: a truncated download, or a file that is not an image
     * at all. It must not parse as an empty but valid disc. */
    memset(&m, 0, sizeof(m));
    m.fail_at = -1;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EFORMAT);

    /* Valid magic, wrong logical block size. Every LBA computed from
     * such an image points somewhere else, so it is refused rather
     * than read at the wrong offsets. */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    put16(m.data + (size_t)LBA_PVD * SECSZ + 128, 2352);
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EFORMAT);
}

static void check_io_error_is_not_a_verdict(void)
{
    img_t m;
    atlas_disc_info_t info;

    /*
     * A USB stick that stops responding mid-probe must be reported as
     * I/O, never as "this disc has no game on it". The two produce very
     * different advice for the user, and the wrong one sends them
     * hunting for a bad dump when the cable is loose.
     */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    m.fail_at = LBA_PVD;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EIO);

    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    m.fail_at = LBA_ROOT;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EIO);

    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    m.fail_at = LBA_CNF;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EIO);
}

static void check_malformed_records(void)
{
    img_t m;
    atlas_disc_info_t info;
    unsigned char *root;

    /*
     * A directory record whose length runs past its sector. Trusting it
     * would walk off the end of the buffer - a crash on the console,
     * from a file the user merely browsed to.
     */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    root = m.data + (size_t)LBA_ROOT * SECSZ;
    {
        /*
         * A root directory that is entirely a chain of records whose
         * lengths carry the walk past the end of the sector. A length
         * byte tops out at 255, so reaching the edge takes several of
         * them - which is how a damaged directory actually presents.
         *
         * The whole sector is overwritten rather than one record: a
         * malformation placed after a match is never reached, so a
         * check that leaves SYSTEM.CNF findable proves nothing about
         * the bounds test it claims to be about.
         *
         * Without that test, the step past 2048 reads a record header
         * from beyond a stack buffer.
         */
        int off = 0;

        memset(root, 0, SECSZ);

        while (off + 254 <= SECSZ) {
            root[off] = 254;
            root[off + 32] = 8;   /* a plausible name length */
            off += 254;
        }

        root[off] = 254;          /* this one runs off the end */
        root[off + 32] = 8;
    }

    /* The disc survives, and reports honestly that it found no game
     * rather than launching whatever lay past the end of the buffer. */
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(info.id[0] == '\0');

    /* A record too short to hold its own fixed fields. */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    root = m.data + (size_t)LBA_ROOT * SECSZ;
    root[0] = 4;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);

    /* A name length reaching past the end of its own record. */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    root = m.data + (size_t)LBA_ROOT * SECSZ;
    root[32] = 200;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);

    /* A root directory of no length is structurally broken: there is
     * nothing to walk and nothing that could be found. */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");
    put32(m.data + (size_t)LBA_PVD * SECSZ + 156 + 10, 0);
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EFORMAT);
}

static void check_oversized_boot2(void)
{
    img_t m;
    atlas_disc_info_t info;
    char cnf[512];
    int i;

    /*
     * A BOOT2 longer than the field that holds it. Truncating it would
     * hand the loader a path that does not exist; leaving it empty
     * reports the disc as unlaunchable, which is the truth.
     */
    strcpy(cnf, "BOOT2 = cdrom0:\\");
    for (i = 0; i < 200; i++)
        strcat(cnf, "A");
    strcat(cnf, ".ELF;1\n");

    build(&m, "GAME", cnf);
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(info.boot[0] == '\0');
    assert(info.id[0] == '\0');
}

static void check_pvd_not_first(void)
{
    img_t m;
    atlas_disc_info_t info;
    unsigned char *first = m.data;

    /*
     * An image with a boot record ahead of the primary descriptor. This
     * is an ordinary disc, not a broken one, and assuming the PVD sits
     * at sector 16 would report it as "not an ISO".
     */
    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");

    first = m.data + (size_t)LBA_PVD * SECSZ;
    memmove(first + SECSZ, first, SECSZ);   /* PVD moves to 17 */

    first[0] = 0;                            /* boot record at 16 */
    memcpy(first + 1, "CD001", 5);
    memset(first + 6, 0, SECSZ - 6);

    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_OK);
    assert(strcmp(info.id, "SLUS-20902") == 0);
}

static void check_arguments(void)
{
    img_t m;
    atlas_disc_info_t info;

    build(&m, "GAME", "BOOT2 = cdrom0:\\SLUS_209.02;1\n");

    assert(atlas_disc_probe(NULL, &m, &info) == ATLAS_EINVAL);
    assert(atlas_disc_probe(img_read, &m, NULL) == ATLAS_EINVAL);

    /*
     * On a failed probe `out` is untouched, so a caller that ignored
     * the return value reads its own stale bytes rather than a
     * half-filled struct that looks like a real disc.
     */
    memset(&info, 0xAB, sizeof(info));
    m.fail_at = LBA_PVD;
    assert(atlas_disc_probe(img_read, &m, &info) == ATLAS_EIO);
    {
        atlas_disc_info_t untouched;
        memset(&untouched, 0xAB, sizeof(untouched));
        assert(memcmp(&info, &untouched, sizeof(info)) == 0);
    }
}

int main(void)
{
    check_id_normalize();
    check_region();
    check_probe_basic();
    check_line_endings();
    check_vmode_overrides_prefix();
    check_data_disc();
    check_not_iso();
    check_io_error_is_not_a_verdict();
    check_malformed_records();
    check_oversized_boot2();
    check_pvd_not_first();
    check_arguments();

    printf("test_disc: OK\n");
    return 0;
}
