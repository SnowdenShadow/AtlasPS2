/*
 * AtlasPS2 - tests/test_config.c
 *
 * The configuration mapping: what a text file turns into, what a bad
 * value does, and whether a file written by this build reads back as
 * the settings that wrote it.
 *
 * That last one is the check worth having. A save/load pair that loses
 * a field loses the user's settings silently, and on a console there is
 * nowhere to notice it until the setting is gone.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/config.h"

static void test_defaults(void)
{
    atlas_config_t c;

    atlas_config_defaults(&c);

    assert(c.lang == ATLAS_LANG_EN);
    assert(c.startup == ATLAS_STARTUP_HOME);
    assert(strcmp(c.theme, "default") == 0);
    assert(c.video.mode == ATLAS_VMODE_AUTO);
    assert(c.video.aspect == ATLAS_ASPECT_AUTO);
    assert(c.animations == 1);
    assert(c.sounds == 1);

    /* A fresh install must land on the menu: booting straight into
     * someone else's program leaves no way to reach the settings. */
    assert(c.default_app[0] == '\0');
    assert(c.timeout == 0);
}

static void test_typical_file(void)
{
    static const char text[] =
        "# a comment\n"
        "[system]\n"
        "language = fr\n"
        "theme=midnight\n"
        "startup=apps\n"
        "\r\n"
        "[video]\r\n"
        "mode=pal\r\n"
        "aspect=16:9\r\n"
        "overscan_x=12\r\n"
        "offset_y=-4\r\n"
        "\n"
        "[ui]\n"
        "animations=off\n"
        "sounds=yes\n"
        "\n"
        "[boot]\n"
        "default_app=mass:/APPS/OPL.ELF\n"
        "timeout=5\n";
    atlas_config_t c;
    int bad = -1;

    atlas_config_parse(&c, text, (int)strlen(text), &bad);

    assert(bad == 0);
    assert(c.lang == ATLAS_LANG_FR);
    assert(strcmp(c.theme, "midnight") == 0);
    assert(c.startup == ATLAS_STARTUP_APPS);
    assert(c.video.mode == ATLAS_VMODE_PAL);
    assert(c.video.aspect == ATLAS_ASPECT_16_9);
    assert(c.video.overscan_x == 12);
    assert(c.video.offset_y == -4);
    assert(c.animations == 0);
    assert(c.sounds == 1);
    assert(strcmp(c.default_app, "mass:/APPS/OPL.ELF") == 0);
    assert(c.timeout == 5);

    /* Untouched keys keep their defaults rather than becoming zero. */
    assert(c.video.overscan_y == 0);
    assert(c.video.offset_x == 0);
}

static void test_partial_file_is_whole_config(void)
{
    static const char text[] = "[system]\nlanguage=fr\n";
    atlas_config_t c;

    atlas_config_parse(&c, text, (int)strlen(text), NULL);

    assert(c.lang == ATLAS_LANG_FR);
    assert(c.animations == 1);           /* default, not 0 */
    assert(strcmp(c.theme, "default") == 0);
}

static void test_unknown_is_ignored_not_fatal(void)
{
    /* A file from a later version. Everything this build understands
     * must still apply. */
    static const char text[] =
        "[system]\n"
        "language=fr\n"
        "quantum_flux=42\n"
        "[network]\n"
        "ip=192.168.1.2\n"
        "[ui]\n"
        "sounds=off\n";
    atlas_config_t c;
    int bad = -1;

    atlas_config_parse(&c, text, (int)strlen(text), &bad);

    assert(bad == 0);                    /* parsable, just unrecognised */
    assert(c.lang == ATLAS_LANG_FR);
    assert(c.sounds == 0);
}

static void test_out_of_range_is_clamped(void)
{
    static const char text[] =
        "[video]\n"
        "overscan_x=900\n"
        "overscan_y=-5\n"
        "offset_x=1000\n"
        "offset_y=-1000\n"
        "[boot]\n"
        "timeout=99999\n";
    atlas_config_t c;

    atlas_config_parse(&c, text, (int)strlen(text), NULL);

    assert(c.video.overscan_x == 64);
    assert(c.video.overscan_y == 0);
    assert(c.video.offset_x == 32);
    assert(c.video.offset_y == -32);
    assert(c.timeout == 30);
}

static void test_garbage_number_keeps_default(void)
{
    /* atoi("left") is 0, which is a legal offset - so a typo would
     * become a setting the user never chose. It must not. */
    static const char text[] =
        "[video]\n"
        "offset_x=left\n"
        "overscan_y=12px\n"
        "[boot]\n"
        "timeout=soon\n";
    atlas_config_t c;

    atlas_config_parse(&c, text, (int)strlen(text), NULL);

    assert(c.video.offset_x == 0);
    assert(c.video.overscan_y == 0);
    assert(c.timeout == 0);
}

static void test_bool_spellings(void)
{
    static const char on[] =
        "[ui]\nanimations=true\nsounds=1\n";
    static const char off[] =
        "[ui]\nanimations=no\nsounds=off\n";
    static const char nonsense[] =
        "[ui]\nanimations=maybe\n";
    atlas_config_t c;

    atlas_config_parse(&c, on, (int)strlen(on), NULL);
    assert(c.animations == 1 && c.sounds == 1);

    atlas_config_parse(&c, off, (int)strlen(off), NULL);
    assert(c.animations == 0 && c.sounds == 0);

    /* Unrecognised keeps whatever was there: guessing either way would
     * be a setting the user did not make. */
    atlas_config_parse(&c, nonsense, (int)strlen(nonsense), NULL);
    assert(c.animations == 1);
}

static void test_unknown_video_mode_is_auto(void)
{
    /* A mode this build does not have must still produce a picture. */
    static const char text[] = "[video]\nmode=1080p\naspect=cinema\n";
    atlas_config_t c;

    atlas_config_parse(&c, text, (int)strlen(text), NULL);

    assert(c.video.mode == ATLAS_VMODE_AUTO);
    assert(c.video.aspect == ATLAS_ASPECT_AUTO);
}

static void test_aspect_spellings(void)
{
    static const char a[] = "[video]\naspect=16/9\n";
    static const char b[] = "[video]\naspect=widescreen\n";
    atlas_config_t c;

    atlas_config_parse(&c, a, (int)strlen(a), NULL);
    assert(c.video.aspect == ATLAS_ASPECT_16_9);

    atlas_config_parse(&c, b, (int)strlen(b), NULL);
    assert(c.video.aspect == ATLAS_ASPECT_16_9);
}

static void test_overlong_default_app_is_dropped(void)
{
    /*
     * Not truncated: a shortened path names a different file, and this
     * is the one path AtlasPS2 launches without being asked.
     */
    char text[ATLAS_CFG_PATH_MAX + 64];
    atlas_config_t c;
    int i, n;

    n = snprintf(text, sizeof(text), "[boot]\ndefault_app=mass:/");
    for (i = 0; i < ATLAS_CFG_PATH_MAX; i++)
        text[n++] = 'A';
    text[n++] = '\n';

    atlas_config_parse(&c, text, n, NULL);

    assert(c.default_app[0] == '\0');
}

static void test_format_then_parse_roundtrip(void)
{
    atlas_config_t a, b;
    char text[ATLAS_CFG_FILE_MAX];
    int n;

    atlas_config_defaults(&a);
    a.lang    = ATLAS_LANG_FR;
    a.startup = ATLAS_STARTUP_APPS;
    snprintf(a.theme, sizeof(a.theme), "%s", "midnight");
    a.video.mode       = ATLAS_VMODE_480P;
    a.video.aspect     = ATLAS_ASPECT_16_9;
    a.video.offset_x   = -7;
    a.video.offset_y   = 3;
    a.video.overscan_x = 20;
    a.video.overscan_y = 9;
    a.animations = 0;
    a.sounds     = 1;
    snprintf(a.default_app, sizeof(a.default_app), "%s",
             "mc0:/ATLAS/APPS/THING.ELF");
    a.timeout = 12;

    n = atlas_config_format(&a, text, (int)sizeof(text));
    assert(n > 0);
    assert(n < (int)sizeof(text));

    atlas_config_parse(&b, text, n, NULL);

    /* Every field, one by one: a memcmp would pass on padding and hide
     * a field the formatter forgot to write. */
    assert(b.lang == a.lang);
    assert(b.startup == a.startup);
    assert(strcmp(b.theme, a.theme) == 0);
    assert(b.video.mode == a.video.mode);
    assert(b.video.aspect == a.video.aspect);
    assert(b.video.offset_x == a.video.offset_x);
    assert(b.video.offset_y == a.video.offset_y);
    assert(b.video.overscan_x == a.video.overscan_x);
    assert(b.video.overscan_y == a.video.overscan_y);
    assert(b.animations == a.animations);
    assert(b.sounds == a.sounds);
    assert(strcmp(b.default_app, a.default_app) == 0);
    assert(b.timeout == a.timeout);
}

static void test_format_refuses_to_truncate(void)
{
    /* A short buffer must fail, not emit half a file: a document that
     * ends mid-value parses back as a different setting. */
    atlas_config_t c;
    char small[64];

    atlas_config_defaults(&c);
    assert(atlas_config_format(&c, small, (int)sizeof(small)) == -1);
}

static void test_bad_arguments(void)
{
    atlas_config_t c;
    char buf[64];

    atlas_config_defaults(&c);

    assert(atlas_config_format(NULL, buf, (int)sizeof(buf)) == -1);
    assert(atlas_config_format(&c, NULL, 10) == -1);
    assert(atlas_config_format(&c, buf, 0) == -1);

    assert(atlas_config_set(NULL, "ui", "sounds", "0") == 0);
    assert(atlas_config_set(&c, "ui", "sounds", NULL) == 0);

    /* Must not crash or read past the end. */
    atlas_config_parse(&c, NULL, 0, NULL);
    assert(c.animations == 1);
}

int main(void)
{
    test_defaults();
    test_typical_file();
    test_partial_file_is_whole_config();
    test_unknown_is_ignored_not_fatal();
    test_out_of_range_is_clamped();
    test_garbage_number_keeps_default();
    test_bool_spellings();
    test_unknown_video_mode_is_auto();
    test_aspect_spellings();
    test_overlong_default_app_is_dropped();
    test_format_then_parse_roundtrip();
    test_format_refuses_to_truncate();
    test_bad_arguments();

    printf("test_config: all checks passed\n");
    return 0;
}
