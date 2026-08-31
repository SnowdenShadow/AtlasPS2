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

int main(void)
{
    test_basic_join();
    test_no_double_separator();
    test_empty_relative_is_the_root();
    test_refuses_rather_than_truncates();
    test_exact_fit();
    test_bad_arguments();

    printf("path: all checks passed\n");
    return 0;
}
