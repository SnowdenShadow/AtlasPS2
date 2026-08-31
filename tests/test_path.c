/*
 * AtlasPS2 - test_path.c
 *
 * Self-check for path joining - the function standing between the
 * installer, the file manager and the wrong file. The refusal cases
 * matter more than the success ones: a truncated path names a
 * different file that may well exist, and these callers delete and
 * overwrite.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/path.h"

/* Fill with a sentinel so an untouched buffer is detectable. */
static void poison(char *buf, int n)
{
    memset(buf, '#', (size_t)n);
}

static void test_basic_join(void)
{
    char buf[64];

    assert(atlas_path_join("mc0:", "ATLAS/ATLAS.INI", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "mc0:/ATLAS/ATLAS.INI") == 0);

    assert(atlas_path_join("mass:", "BOOT.ELF", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mass:/BOOT.ELF") == 0);
}

static void test_no_double_separator(void)
{
    char buf[64];

    /* A caller writing "/ATLAS" must not produce "mc0://ATLAS". */
    assert(atlas_path_join("mc0:", "/ATLAS", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mc0:/ATLAS") == 0);

    assert(atlas_path_join("mc0:", "///ATLAS", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mc0:/ATLAS") == 0);

    /* A base already ending in '/' does not get a second one either. */
    assert(atlas_path_join("mass:/", "APPS", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mass:/APPS") == 0);

    assert(atlas_path_join("mass:/", "/APPS", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mass:/APPS") == 0);
}

static void test_empty_relative_is_the_root(void)
{
    char buf[64];

    assert(atlas_path_join("mc0:", "", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mc0:") == 0);

    /* Slashes only is still the root, not a trailing separator. */
    assert(atlas_path_join("mc0:", "/", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mc0:") == 0);
}

static void test_refuses_rather_than_truncates(void)
{
    char buf[16];

    /* "mc0:/ATLAS/ATLAS.INI" is 20 chars + NUL: it cannot fit in 16. */
    poison(buf, sizeof(buf));
    assert(atlas_path_join("mc0:", "ATLAS/ATLAS.INI", buf, sizeof(buf))
           == ATLAS_EINVAL);

    /*
     * The buffer must be untouched, not partially written: a caller
     * that ignores the return value must not find "mc0:/ATLAS/ATL" -
     * a real, different file - sitting in it.
     */
    {
        int i;
        for (i = 0; i < (int)sizeof(buf); i++)
            assert(buf[i] == '#');
    }
}

static void test_exact_fit(void)
{
    char buf[11];  /* "mc0:/ATLAS" is 10 chars + NUL */

    assert(atlas_path_join("mc0:", "ATLAS", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "mc0:/ATLAS") == 0);

    /* One byte short must refuse, not drop the terminator. */
    assert(atlas_path_join("mc0:", "ATLAS", buf, 10) == ATLAS_EINVAL);
}

static void test_bad_arguments(void)
{
    char buf[32];

    assert(atlas_path_join(NULL, "x", buf, sizeof(buf)) == ATLAS_EINVAL);
    assert(atlas_path_join("mc0:", NULL, buf, sizeof(buf)) == ATLAS_EINVAL);
    assert(atlas_path_join("mc0:", "x", NULL, sizeof(buf)) == ATLAS_EINVAL);
    assert(atlas_path_join("mc0:", "x", buf, 0) == ATLAS_EINVAL);
    assert(atlas_path_join("mc0:", "x", buf, -1) == ATLAS_EINVAL);
}

static void test_pretty_name(void)
{
    char buf[32];

    /* The common case: a bare ELF with no metadata beside it. */
    assert(atlas_path_pretty_name("mass:/APPS/OPL.ELF", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "OPL") == 0);

    /* Separators are turned into spaces so a row reads as a title. */
    assert(atlas_path_pretty_name("SIMPLE_MEDIA_SYSTEM.ELF", buf,
                                  sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, "SIMPLE MEDIA SYSTEM") == 0);

    assert(atlas_path_pretty_name("ps2-hdd-manager.elf", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "ps2 hdd manager") == 0);

    /* Case is left alone: "uLaunchELF" is how its author writes it. */
    assert(atlas_path_pretty_name("uLaunchELF.ELF", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "uLaunchELF") == 0);

    /* Only the last dot is an extension. */
    assert(atlas_path_pretty_name("OPL.v1.2.ELF", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "OPL.v1.2") == 0);

    /* A backslash counts as a separator too - the file may have been
     * copied from a PC and carry one in a stored path. */
    assert(atlas_path_pretty_name("APPS\\BOOT.ELF", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "BOOT") == 0);

    /* No extension at all is fine. */
    assert(atlas_path_pretty_name("mc0:/BOOT", buf, sizeof(buf))
           == ATLAS_OK);
    assert(strcmp(buf, "BOOT") == 0);
}

static void test_pretty_name_never_yields_empty(void)
{
    char buf[32];

    /*
     * A row labelled "" is one the user cannot identify or trust. Every
     * degenerate input must still produce something.
     */
    assert(atlas_path_pretty_name(".ELF", buf, sizeof(buf)) == ATLAS_OK);
    assert(strcmp(buf, ".ELF") == 0);

    assert(atlas_path_pretty_name("mass:/APPS/", buf, sizeof(buf))
           == ATLAS_OK);
    assert(buf[0] != '\0');

    /* The one genuinely empty input. Nothing to salvage, but it must
     * still terminate the buffer rather than leave it as it was. */

    assert(atlas_path_pretty_name("", buf, sizeof(buf)) == ATLAS_OK);
    assert(buf[0] == '\0');
}

static void test_pretty_name_truncates_rather_than_refuses(void)
{
    char buf[8];

    /*
     * The opposite rule from atlas_path_join(): this string is only
     * ever displayed, never opened, so a clipped label costs a squint
     * while a refusal would cost the row entirely.
     */
    assert(atlas_path_pretty_name("VERYLONGAPPLICATIONNAME.ELF", buf,
                                  sizeof(buf)) == ATLAS_OK);
    assert(strlen(buf) == sizeof(buf) - 1);
    assert(strcmp(buf, "VERYLON") == 0);

    assert(atlas_path_pretty_name("X.ELF", NULL, 8) == ATLAS_EINVAL);
    assert(atlas_path_pretty_name(NULL, buf, 8) == ATLAS_EINVAL);
    assert(atlas_path_pretty_name("X.ELF", buf, 0) == ATLAS_EINVAL);
}

int main(void)
{
    test_basic_join();
    test_no_double_separator();
    test_empty_relative_is_the_root();
    test_refuses_rather_than_truncates();
    test_exact_fit();
    test_bad_arguments();
    test_pretty_name();
    test_pretty_name_never_yields_empty();
    test_pretty_name_truncates_rather_than_refuses();

    printf("path: all checks passed\n");
    return 0;
}
