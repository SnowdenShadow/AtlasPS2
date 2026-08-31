/*
 * AtlasPS2 - tests/test_theme.c
 *
 * The theme colour parser.
 *
 * A theme is the one thing a user edits by hand in a text editor and
 * then has to judge by looking at a television across the room. When a
 * colour comes out wrong there is no error message, no log they can
 * read, and nothing to compare against - so the rules about what a
 * value means have to be pinned here, where a mistake is a failing
 * check rather than a panel nobody can read.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/theme.h"

/** The eight bits the GS reads for one channel of a theme colour. */
#define CH_R(c) ((int)((c)       & 0xFF))
#define CH_G(c) ((int)(((c) >> 8)  & 0xFF))
#define CH_B(c) ((int)(((c) >> 16) & 0xFF))
#define CH_A(c) ((int)(((c) >> 24) & 0xFF))

static void test_builtin_is_complete(void)
{
    const atlas_theme_t *b = atlas_theme_builtin();
    const u64 *p = (const u64 *)b;
    int i;

    /*
     * Every field initialised. The built-in table is positional - it
     * follows ATLAS_THEME_FIELDS by order alone - so a colour added to
     * the X-macro without a matching row would leave a zero here: a
     * transparent black that draws as nothing at all.
     */
    for (i = 0; i < ATLAS_THEME_FIELD_COUNT; i++)
        assert(p[i] != 0);

    /* And every one of them opaque, or the interface is see-through. */
    for (i = 0; i < ATLAS_THEME_FIELD_COUNT; i++)
        assert(CH_A(p[i]) == ATLAS_ALPHA_OPAQUE);

    assert(ATLAS_THEME_FIELD_COUNT == 15);
}

static void test_active_defaults_to_builtin(void)
{
    /* Nothing loaded yet: the palette exists anyway. This is what makes
     * a missing theme file cosmetic rather than fatal. */
    assert(atlas_theme() == atlas_theme_builtin());
    assert(strcmp(atlas_theme_name(), "") == 0);
}

static void test_color_forms(void)
{
    atlas_theme_t t;

    t = *atlas_theme_builtin();

    /* With and without the '#', upper and lower case. */
    assert(atlas_theme_set_field(&t, "accent", "#123456"));
    assert(CH_R(t.accent) == 0x12);
    assert(CH_G(t.accent) == 0x34);
    assert(CH_B(t.accent) == 0x56);
    assert(CH_A(t.accent) == ATLAS_ALPHA_OPAQUE);

    assert(atlas_theme_set_field(&t, "accent", "abcdef"));
    assert(CH_R(t.accent) == 0xAB);
    assert(CH_G(t.accent) == 0xCD);
    assert(CH_B(t.accent) == 0xEF);

    assert(atlas_theme_set_field(&t, "accent", "#ABCDEF"));
    assert(CH_R(t.accent) == 0xAB);

    /* Eight digits carry alpha. */
    assert(atlas_theme_set_field(&t, "panel", "#10203040"));
    assert(CH_A(t.panel) == 0x40);
}

static void test_alpha_is_clamped_to_the_gs_scale(void)
{
    atlas_theme_t t = *atlas_theme_builtin();

    /*
     * The whole reason this check exists. Every other system on earth
     * spells "opaque" 0xFF, so a theme author will write it - and on
     * the GS that over-saturates rather than getting brighter, turning
     * a panel into a white block. Clamped, not obeyed, and not
     * rejected either: the author's intent is perfectly clear.
     */
    assert(atlas_theme_set_field(&t, "panel", "#102030FF"));
    assert(CH_A(t.panel) == ATLAS_ALPHA_OPAQUE);

    /* Below the ceiling is left exactly as written - a translucent
     * panel is a thing a theme may legitimately want. */
    assert(atlas_theme_set_field(&t, "panel", "#10203020"));
    assert(CH_A(t.panel) == 0x20);

    /* Including fully transparent, which is odd but is not our call. */
    assert(atlas_theme_set_field(&t, "panel", "#10203000"));
    assert(CH_A(t.panel) == 0x00);
}

static void test_bad_values_are_refused(void)
{
    atlas_theme_t t = *atlas_theme_builtin();
    u64 before = t.accent;

    /* Wrong length, either way. */
    assert(!atlas_theme_set_field(&t, "accent", "#12345"));
    assert(!atlas_theme_set_field(&t, "accent", "#1234567"));
    assert(!atlas_theme_set_field(&t, "accent", "#123456789"));
    assert(!atlas_theme_set_field(&t, "accent", ""));
    assert(!atlas_theme_set_field(&t, "accent", "#"));

    /* Not hex. A typo that half-parsed would give a colour the author
     * never chose and could not account for. */
    assert(!atlas_theme_set_field(&t, "accent", "#12345g"));
    assert(!atlas_theme_set_field(&t, "accent", "blue"));
    assert(!atlas_theme_set_field(&t, "accent", "#12 456"));

    /* A refusal changes nothing, so a bad line costs one colour and
     * leaves the other fourteen alone. */
    assert(t.accent == before);

    /* Unknown keys are ignored, not fatal: a theme written for a later
     * version with more colours in it still loads on this one. */
    assert(!atlas_theme_set_field(&t, "sidebar_glow", "#123456"));

    /* Null arguments. */
    assert(!atlas_theme_set_field(NULL, "accent", "#123456"));
    assert(!atlas_theme_set_field(&t, NULL, "#123456"));
    assert(!atlas_theme_set_field(&t, "accent", NULL));
}

static void test_every_field_is_addressable(void)
{
    atlas_theme_t t;
    const u64 *p = (const u64 *)&t;
    int i = 0;

    t = *atlas_theme_builtin();

    /*
     * Set each field by its file name and check the value landed in the
     * matching struct slot. This is what would catch the X-macro naming
     * one field and assigning another - a theme file where "warn" set
     * the error colour and nobody could tell why.
     *
     * Each gets a distinct value so a copy-paste in the table shows up
     * as two slots holding the same number.
     */
#define SET_ONE(field, name)                                        \
    do {                                                            \
        char v[16];                                                 \
        sprintf(v, "#%02X0000", 0x10 + i);                          \
        assert(atlas_theme_set_field(&t, name, v));                 \
        i++;                                                        \
    } while (0);

    ATLAS_THEME_FIELDS(SET_ONE)
#undef SET_ONE

    assert(i == ATLAS_THEME_FIELD_COUNT);

    for (i = 0; i < ATLAS_THEME_FIELD_COUNT; i++)
        assert(CH_R(p[i]) == 0x10 + i);
}

static void test_partial_file_keeps_the_rest(void)
{
    static const char text[] =
        "# AtlasPS2 theme\n"
        "accent = #FF0000\n";

    atlas_theme_t t;
    int applied = -1;

    assert(atlas_theme_parse(&t, text, (int)sizeof(text) - 1, &applied)
           == ATLAS_OK);
    assert(applied == 1);

    assert(CH_R(t.accent) == 0xFF);

    /*
     * The one guarantee that makes hand-editing a theme safe: naming a
     * single colour changes a single colour. Starting from zero instead
     * would give a file like this fourteen invisible fields.
     */
    assert(t.text == atlas_theme_builtin()->text);
    assert(t.bg_top == atlas_theme_builtin()->bg_top);
    assert(t.bar_text == atlas_theme_builtin()->bar_text);
}

static void test_file_tolerance(void)
{
    /* Everything a text editor and a human do to a config file: CRLF,
     * both comment characters, a trailing comment, spacing, a section
     * header, blank lines, and no newline at the end. */
    static const char text[] =
        "\r\n"
        "; a theme\r\n"
        "[colors]\r\n"
        "  bg_top   =   #010203  \r\n"
        "text=#FFFFFF   # the main text\r\n"
        "\r\n"
        "panel = #040506";

    atlas_theme_t t;
    int applied = 0;

    assert(atlas_theme_parse(&t, text, (int)sizeof(text) - 1, &applied)
           == ATLAS_OK);
    assert(applied == 3);

    assert(CH_R(t.bg_top) == 0x01);
    assert(CH_B(t.bg_top) == 0x03);
    assert(CH_R(t.text) == 0xFF);
    assert(CH_G(t.panel) == 0x05);
}

static void test_one_bad_line_costs_one_colour(void)
{
    static const char text[] =
        "accent = #FF0000\n"
        "panel = not a colour\n"
        "this line has no equals sign\n"
        "=#112233\n"
        "text = #00FF00\n";

    atlas_theme_t t;
    int applied = 0;

    assert(atlas_theme_parse(&t, text, (int)sizeof(text) - 1, &applied)
           == ATLAS_OK);

    /* Two good lines out of five, and the damage stops at the bad ones:
     * a theme with a typo in it still loads, minus that colour. */
    assert(applied == 2);
    assert(CH_R(t.accent) == 0xFF);
    assert(CH_G(t.text) == 0xFF);
    assert(t.panel == atlas_theme_builtin()->panel);
}

static void test_parse_rejects_bad_arguments(void)
{
    atlas_theme_t t;

    assert(atlas_theme_parse(NULL, "x", 1, NULL) == ATLAS_EINVAL);
    assert(atlas_theme_parse(&t, NULL, 1, NULL) == ATLAS_EINVAL);
    assert(atlas_theme_parse(&t, "x", -1, NULL) == ATLAS_EINVAL);

    /* An empty file is not a bad file. It is a theme that changes
     * nothing, and it must come back as the built-in palette rather
     * than as an error the caller has to have a plan for. */
    assert(atlas_theme_parse(&t, "", 0, NULL) == ATLAS_OK);
    assert(memcmp(&t, atlas_theme_builtin(), sizeof(t)) == 0);
}

static void test_set_and_clear(void)
{
    atlas_theme_t t = *atlas_theme_builtin();

    assert(atlas_theme_set_field(&t, "accent", "#FF0000"));

    atlas_theme_set(&t);
    assert(atlas_theme() != atlas_theme_builtin());
    assert(CH_R(atlas_theme()->accent) == 0xFF);

    /*
     * The theme was copied, not referenced. Scribbling on the caller's
     * struct afterwards must not reach the live palette - on the
     * console that struct is a stack local in the loader.
     */
    memset(&t, 0, sizeof(t));
    assert(CH_R(atlas_theme()->accent) == 0xFF);

    /* NULL is how Recovery gets a readable interface no matter what is
     * installed, so it has to be a full return to the built-in one. */
    atlas_theme_set(NULL);
    assert(atlas_theme() == atlas_theme_builtin());
    assert(strcmp(atlas_theme_name(), "") == 0);
}

/*
 * The theme that ships in assets/ is the file every theme author starts
 * from by copying it. If a key in it were misspelled the theme would
 * still load - unknown keys are ignored - and the wrong colour would
 * simply stay at its built-in value, which is exactly the kind of
 * mistake that gets copied into every theme anyone writes afterwards.
 */
static void test_shipped_example_sets_everything(void)
{
    static char buf[ATLAS_THEME_FILE_MAX];
    const char *path = "../assets/themes/Midnight/theme.ini";
    atlas_theme_t t;
    FILE *f = fopen(path, "rb");
    int len, applied = 0;

    assert(f != NULL);
    len = (int)fread(buf, 1, sizeof(buf), f);
    fclose(f);

    /* It has to fit in the buffer the console reads it with, or the
     * check passed on a file the PS2 would refuse. */
    assert(len > 0 && len < (int)sizeof(buf));

    assert(atlas_theme_parse(&t, buf, len, &applied) == ATLAS_OK);
    assert(applied == ATLAS_THEME_FIELD_COUNT);

    /* And it is a theme, not a copy of the default under a new name. */
    assert(memcmp(&t, atlas_theme_builtin(), sizeof(t)) != 0);
}

int main(void)
{
    test_builtin_is_complete();
    test_active_defaults_to_builtin();
    test_color_forms();
    test_alpha_is_clamped_to_the_gs_scale();
    test_bad_values_are_refused();
    test_every_field_is_addressable();
    test_partial_file_keeps_the_rest();
    test_file_tolerance();
    test_one_bad_line_costs_one_colour();
    test_parse_rejects_bad_arguments();
    test_shipped_example_sets_everything();
    test_set_and_clear();

    printf("test_theme: all checks passed (%d colours)\n",
           ATLAS_THEME_FIELD_COUNT);
    return 0;
}
