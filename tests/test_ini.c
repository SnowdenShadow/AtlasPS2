/*
 * AtlasPS2 - test_ini.c
 *
 * Self-check for the INI reader. The file it parses is written by a
 * user in a PC text editor, so the interesting cases are the malformed
 * ones: what matters is that a damaged line costs one setting and not
 * the whole file, and that an oversized value is dropped rather than
 * truncated into the name of some other file.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/ini.h"

#define MAX_PAIRS 16

typedef struct {
    int  n;
    char section[MAX_PAIRS][ATLAS_INI_SECTION_MAX];
    char key[MAX_PAIRS][ATLAS_INI_KEY_MAX];
    char value[MAX_PAIRS][ATLAS_INI_VALUE_MAX];
    int  stop_after;   /* 0 = never stop */
} collect_t;

static int collect(void *user, const char *section,
                   const char *key, const char *value)
{
    collect_t *c = (collect_t *)user;

    assert(c->n < MAX_PAIRS);
    snprintf(c->section[c->n], ATLAS_INI_SECTION_MAX, "%s", section);
    snprintf(c->key[c->n], ATLAS_INI_KEY_MAX, "%s", key);
    snprintf(c->value[c->n], ATLAS_INI_VALUE_MAX, "%s", value);
    c->n++;

    return (c->stop_after && c->n >= c->stop_after) ? 1 : 0;
}

static void run(collect_t *c, const char *text, int *bad)
{
    memset(c, 0, sizeof(*c));
    assert(atlas_ini_parse(text, (int)strlen(text), collect, c, bad)
           == ATLAS_OK);
}

static void expect(const collect_t *c, int i, const char *s,
                   const char *k, const char *v)
{
    assert(i < c->n);
    assert(strcmp(c->section[i], s) == 0);
    assert(strcmp(c->key[i], k) == 0);
    assert(strcmp(c->value[i], v) == 0);
}

static void test_typical_app_ini(void)
{
    collect_t c;
    int bad;

    run(&c, "[app]\n"
            "name=Open PS2 Loader\n"
            "elf=OPL.ELF\n"
            "category=Games\n", &bad);

    assert(c.n == 3);
    assert(bad == 0);
    expect(&c, 0, "app", "name", "Open PS2 Loader");
    expect(&c, 1, "app", "elf", "OPL.ELF");
    expect(&c, 2, "app", "category", "Games");
}

static void test_tolerates_real_world_text(void)
{
    collect_t c;
    int bad;

    /* CRLF, comments in both styles, blank lines, indentation and
     * spaces around the '=' - all things a PC editor produces. */
    run(&c, "\r\n"
            "# a comment\r\n"
            "; another\r\n"
            "  [ App ]  \r\n"
            "\r\n"
            "   name  =  Open PS2 Loader  \r\n"
            "ELF=OPL.ELF\r\n", &bad);

    assert(bad == 0);
    assert(c.n == 2);

    /* Section and key are folded; the value is not, because it is a
     * filename and FAT paths are case-sensitive to us. */
    expect(&c, 0, "app", "name", "Open PS2 Loader");
    expect(&c, 1, "app", "elf", "OPL.ELF");
}

static void test_value_keeps_later_equals(void)
{
    collect_t c;
    int bad;

    run(&c, "args=-mode=fast\n", &bad);

    assert(bad == 0);
    expect(&c, 0, "", "args", "-mode=fast");
}

static void test_keys_before_any_section(void)
{
    collect_t c;
    int bad;

    run(&c, "name=Loose\n[app]\nname=Bound\n", &bad);

    assert(bad == 0);
    expect(&c, 0, "", "name", "Loose");
    expect(&c, 1, "app", "name", "Bound");
}

static void test_missing_bracket_is_still_a_section(void)
{
    collect_t c;
    int bad;

    /*
     * Treating it as a broken key would put every following key in the
     * previous section - one typo silently moving all the settings.
     */
    run(&c, "[app\nname=X\n", &bad);

    assert(bad == 0);
    expect(&c, 0, "app", "name", "X");
}

static void test_bad_lines_are_skipped_not_fatal(void)
{
    collect_t c;
    int bad;

    run(&c, "garbage without an equals\n"
            "=novalue\n"
            "name=Kept\n"
            "   \n"
            "also bad\n"
            "elf=Kept.ELF\n", &bad);

    assert(bad == 3);   /* the two garbage lines and the empty key   */
    assert(c.n == 2);   /* the good keys either side still arrived   */
    expect(&c, 0, "", "name", "Kept");
    expect(&c, 1, "", "elf", "Kept.ELF");
}

static void test_oversized_is_dropped_not_truncated(void)
{
    collect_t c;
    char text[ATLAS_INI_VALUE_MAX + 64];
    int bad, i;

    /* A value one byte too long must not arrive shortened: it would
     * name a real, different file. */
    strcpy(text, "elf=");
    for (i = 0; i < ATLAS_INI_VALUE_MAX; i++)
        text[4 + i] = 'A';
    text[4 + ATLAS_INI_VALUE_MAX] = '\n';
    text[5 + ATLAS_INI_VALUE_MAX] = '\0';

    run(&c, text, &bad);
    assert(bad == 1);
    assert(c.n == 0);

    /* Exactly at the limit (cap - 1 chars + NUL) is accepted. */
    text[4 + ATLAS_INI_VALUE_MAX - 1] = '\n';
    text[4 + ATLAS_INI_VALUE_MAX] = '\0';
    run(&c, text, &bad);
    assert(bad == 0);
    assert(c.n == 1);
    assert((int)strlen(c.value[0]) == ATLAS_INI_VALUE_MAX - 1);
}

static void test_no_trailing_newline(void)
{
    collect_t c;
    int bad;

    run(&c, "name=Last", &bad);
    assert(bad == 0);
    expect(&c, 0, "", "name", "Last");
}

static void test_callback_can_stop(void)
{
    collect_t c;
    int bad;

    memset(&c, 0, sizeof(c));
    c.stop_after = 2;
    assert(atlas_ini_parse("a=1\nb=2\nc=3\n", 12, collect, &c, &bad)
           == ATLAS_OK);
    assert(c.n == 2);
}

static void test_length_is_honoured(void)
{
    collect_t c;
    int bad;

    /* The parser must read `len` bytes, not to a terminator: buffers
     * come from a file read and may hold whatever followed. */
    memset(&c, 0, sizeof(c));
    assert(atlas_ini_parse("a=1\nb=2\n", 4, collect, &c, &bad) == ATLAS_OK);
    assert(c.n == 1);
    expect(&c, 0, "", "a", "1");
}

static void test_bad_arguments(void)
{
    collect_t c;

    memset(&c, 0, sizeof(c));
    assert(atlas_ini_parse(NULL, 4, collect, &c, NULL) == ATLAS_EINVAL);
    assert(atlas_ini_parse("a=1", 3, NULL, &c, NULL) == ATLAS_EINVAL);
    assert(atlas_ini_parse("a=1", -1, collect, &c, NULL) == ATLAS_EINVAL);

    /* Empty input is valid and yields nothing. */
    memset(&c, 0, sizeof(c));
    assert(atlas_ini_parse("", 0, collect, &c, NULL) == ATLAS_OK);
    assert(c.n == 0);
}

int main(void)
{
    test_typical_app_ini();
    test_tolerates_real_world_text();
    test_value_keeps_later_equals();
    test_keys_before_any_section();
    test_missing_bracket_is_still_a_section();
    test_bad_lines_are_skipped_not_fatal();
    test_oversized_is_dropped_not_truncated();
    test_no_trailing_newline();
    test_callback_can_stop();
    test_length_is_honoured();
    test_bad_arguments();

    printf("ini: all checks passed\n");
    return 0;
}
