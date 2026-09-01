/*
 * AtlasPS2 - test_btconf.c
 *
 * The IOP boot list filter.
 *
 * This check is the reason atlas_btconf_filter() takes a buffer instead
 * of reading rom0: itself. What it decides is which modules the IOP
 * boots with, and every way of getting it wrong lands after the last
 * screen the user will see: a line not removed is our drive emulation
 * loading and never being called, and a line removed that should not
 * have been is an IOP missing something a game needs, on a console
 * revision we do not own.
 *
 * So the lists below include the shapes a real IOPBTCONF has - CRLF, no
 * trailing newline, control lines beginning with '@', leading blanks -
 * rather than only the tidy one.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/btconf.h"

static int s_fail;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);      \
            printf(__VA_ARGS__);                             \
            printf("\n");                                    \
            s_fail = 1;                                      \
        }                                                    \
    } while (0)

/** Compare the filtered output against an expected string. */
static void expect_text(const atlas_btconf_t *bc, const char *want,
                        const char *what)
{
    int n = (int)strlen(want);

    CHECK(bc->len == n, "%s: length %d, wanted %d", what, bc->len, n);

    if (bc->len == n)
        CHECK(memcmp(bc->text, want, (size_t)n) == 0,
              "%s: text differs", what);
}

static void test_ordinary(void)
{
    static const char src[] =
        "@2000000\n"
        "IOPBTCONF\n"
        "SYSMEM\n"
        "LOADCORE\n"
        "EXCEPMAN\n"
        "CDVDMAN\n"
        "CDVDFSV\n"
        "SIFCMD\n";

    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter(src, (int)strlen(src), &bc) == ATLAS_OK,
          "ordinary list refused");

    expect_text(&bc,
                "@2000000\n"
                "IOPBTCONF\n"
                "SYSMEM\n"
                "LOADCORE\n"
                "EXCEPMAN\n"
                "SIFCMD\n",
                "ordinary");

    CHECK(bc.removed_count == 2, "removed %d, wanted 2", bc.removed_count);
    CHECK(strcmp(bc.removed[0], "CDVDMAN") == 0,
          "removed[0] = %s", bc.removed[0]);
    CHECK(strcmp(bc.removed[1], "CDVDFSV") == 0,
          "removed[1] = %s", bc.removed[1]);
}

/* A ROM file is not a text file we control. CRLF endings must not leave
 * a carriage return inside the module name the comparison sees, and
 * must not leave a stray one in the output either. */
static void test_crlf(void)
{
    static const char src[] =
        "SYSMEM\r\n"
        "CDVDMAN\r\n"
        "SIFCMD\r\n";

    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter(src, (int)strlen(src), &bc) == ATLAS_OK,
          "CRLF list refused");
    expect_text(&bc, "SYSMEM\r\nSIFCMD\r\n", "crlf");
    CHECK(bc.removed_count == 1, "crlf: removed %d", bc.removed_count);
}

/* The last line need not end in a newline, and dropping it must not
 * take the previous line's terminator with it. */
static void test_no_trailing_newline(void)
{
    static const char a[] = "SYSMEM\nCDVDMAN";
    static const char b[] = "CDVDMAN\nSYSMEM";

    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter(a, (int)strlen(a), &bc) == ATLAS_OK,
          "unterminated list refused");
    expect_text(&bc, "SYSMEM\n", "unterminated tail");

    CHECK(atlas_btconf_filter(b, (int)strlen(b), &bc) == ATLAS_OK,
          "unterminated list refused (b)");
    expect_text(&bc, "SYSMEM", "unterminated head");
}

/* Leading blanks and lower case are the file's business, not ours. */
static void test_spelling(void)
{
    static const char src[] =
        "  cdvdman\n"
        "\tCdVdFsV\n"
        "SYSMEM\n";

    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter(src, (int)strlen(src), &bc) == ATLAS_OK,
          "spelling list refused");
    expect_text(&bc, "SYSMEM\n", "spelling");
    CHECK(bc.removed_count == 2, "spelling: removed %d", bc.removed_count);

    /* Recorded canonically, whatever the file wrote: the name is about
     * to be pasted after "rom0:". */
    CHECK(strcmp(bc.removed[0], "CDVDMAN") == 0,
          "spelling: removed[0] = %s", bc.removed[0]);
    CHECK(strcmp(bc.removed[1], "CDVDFSV") == 0,
          "spelling: removed[1] = %s", bc.removed[1]);
}

/*
 * The dangerous direction. A module we have not heard of that merely
 * begins with a name we have must stay: removing it is an IOP booting
 * without something a revision needs, and nothing says so.
 */
static void test_near_misses(void)
{
    static const char src[] =
        "CDVDMAN2\n"
        "CDVDMANX\n"
        "XCDVDMAN\n"
        "CDVDMA\n"
        "CDVDMAN ARG\n"
        "CDVDMAN\n";

    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter(src, (int)strlen(src), &bc) == ATLAS_OK,
          "near-miss list refused");

    /* "CDVDMAN ARG" is the real one with an argument: first token
     * matches, so it goes. The other four stay. */
    expect_text(&bc,
                "CDVDMAN2\n"
                "CDVDMANX\n"
                "XCDVDMAN\n"
                "CDVDMA\n",
                "near misses");

    CHECK(bc.removed_count == 2, "near misses: removed %d",
          bc.removed_count);
}

/* Blank lines, comments and anything else are copied through: this
 * knows what it removes and nothing else about the format. */
static void test_passthrough(void)
{
    static const char src[] =
        "\n"
        "# a comment\n"
        "@2000000\n"
        "\n"
        "CDVDMAN\n"
        "   \n";

    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter(src, (int)strlen(src), &bc) == ATLAS_OK,
          "passthrough list refused");
    expect_text(&bc, "\n# a comment\n@2000000\n\n   \n", "passthrough");
}

/*
 * A list with no cdvdman is a list we did not understand. Acting on it
 * would be handing the IOP a filtered copy of a file whose format we
 * guessed wrong, after the last screen.
 */
static void test_refusals(void)
{
    static const char none[] = "SYSMEM\nLOADCORE\n";
    static const char only_fsv[] = "CDVDFSV\nSYSMEM\n";

    atlas_btconf_t bc;
    char big[ATLAS_BTCONF_MAX + 64];
    int i;

    CHECK(atlas_btconf_filter(none, (int)strlen(none), &bc) == ATLAS_EFORMAT,
          "list without cdvdman accepted");

    /* cdvdfsv alone is not enough: cdvdman is the name we are taking,
     * and a list naming only its client is one we have misread. */
    CHECK(atlas_btconf_filter(only_fsv, (int)strlen(only_fsv), &bc)
              == ATLAS_EFORMAT,
          "list with only cdvdfsv accepted");

    CHECK(atlas_btconf_filter(NULL, 0, &bc) == ATLAS_EINVAL,
          "null source accepted");
    CHECK(atlas_btconf_filter(none, -1, &bc) == ATLAS_EINVAL,
          "negative length accepted");
    CHECK(atlas_btconf_filter(none, 4, NULL) == ATLAS_EINVAL,
          "null output accepted");

    /* Too large is refused, not truncated: half a boot list is an IOP
     * missing whichever modules came after the cut. */
    for (i = 0; i < (int)sizeof(big); i++)
        big[i] = (i % 8 == 7) ? '\n' : 'A';

    CHECK(atlas_btconf_filter(big, (int)sizeof(big), &bc) == ATLAS_ENOMEM,
          "oversized list accepted");
}

/* An empty file has no cdvdman in it, so it is refused for that reason
 * rather than crashing on the way there. */
static void test_empty(void)
{
    atlas_btconf_t bc;

    CHECK(atlas_btconf_filter("", 0, &bc) == ATLAS_EFORMAT,
          "empty list accepted");
}

int main(void)
{
    test_ordinary();
    test_crlf();
    test_no_trailing_newline();
    test_spelling();
    test_near_misses();
    test_passthrough();
    test_refusals();
    test_empty();

    if (s_fail) {
        printf("test_btconf: FAILED\n");
        return 1;
    }

    printf("test_btconf: OK\n");
    return 0;
}
