/*
 * AtlasPS2 - test_profile.c
 *
 * Per-title profiles: what each key means, what an unset field is, and
 * the filename a game ID maps to.
 *
 * The reason this is worth checking off-console: every field here has
 * an "unset" state that is not its zero value, and collapsing the two
 * is invisible. A profile that only sets the aspect would silently
 * reset the screen trim someone spent ten minutes on, and they would
 * find out by looking at their television, months later, with no idea
 * which change did it.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/profile.h"
#include "atlas/config.h"   /* ATLAS_CFG_OFFSET_LIMIT: the clamp is shared */
#include "atlas/ini.h"      /* ATLAS_INI_VALUE_MAX, for the over-long path */

/* ------------------------------------------------------------------ */

static void parse(atlas_profile_t *p, const char *text)
{
    assert(atlas_profile_parse(p, text, (int)strlen(text)) == ATLAS_OK);
}

static void check_defaults(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);

    assert(p.mode == ATLAS_VMODE_COUNT);
    assert(p.aspect == ATLAS_ASPECT_COUNT);
    assert(p.offset_x == ATLAS_PROFILE_UNSET);
    assert(p.offset_y == ATLAS_PROFILE_UNSET);
    assert(p.widescreen == ATLAS_PROFILE_UNSET);
    assert(p.launch_app[0] == '\0');
    assert(atlas_profile_is_empty(&p));

    /*
     * AUTO is a real choice, not the absence of one. If these were
     * equal, a profile saying "video_mode = auto" could not override a
     * global setting of PAL - which is exactly what a user picks it
     * for.
     */
    assert((int)ATLAS_VMODE_AUTO != (int)ATLAS_VMODE_COUNT);
    assert((int)ATLAS_ASPECT_AUTO != (int)ATLAS_ASPECT_COUNT);
}

static void check_every_key(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);
    parse(&p,
          "video_mode = pal\n"
          "aspect_ratio = 16:9\n"
          "offset_x = -8\n"
          "offset_y = 4\n"
          "widescreen = yes\n"
          "launch_app = mc0:/APPS/OPL.ELF\n");

    assert(p.mode == ATLAS_VMODE_PAL);
    assert(p.aspect == ATLAS_ASPECT_16_9);
    assert(p.offset_x == -8);
    assert(p.offset_y == 4);
    assert(p.widescreen == 1);
    assert(strcmp(p.launch_app, "mc0:/APPS/OPL.ELF") == 0);
    assert(!atlas_profile_is_empty(&p));
}

static void check_partial_leaves_the_rest(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);
    parse(&p, "aspect_ratio = 4:3\n");

    assert(p.aspect == ATLAS_ASPECT_4_3);

    /* The whole point of the unset marker. */
    assert(p.mode == ATLAS_VMODE_COUNT);
    assert(p.offset_x == ATLAS_PROFILE_UNSET);
    assert(p.offset_y == ATLAS_PROFILE_UNSET);
    assert(p.widescreen == ATLAS_PROFILE_UNSET);
}

static void check_offset_zero_is_not_unset(void)
{
    atlas_profile_t p;
    atlas_video_cfg_t cfg;

    atlas_profile_defaults(&p);
    parse(&p, "offset_x = 0\n");

    assert(p.offset_x == 0);
    assert(p.offset_x != ATLAS_PROFILE_UNSET);

    /* "Centre it" must actually reach the video settings. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.offset_x = 12;
    cfg.offset_y = 12;

    atlas_profile_apply_video(&p, &cfg);

    assert(cfg.offset_x == 0);    /* set by the profile   */
    assert(cfg.offset_y == 12);   /* untouched by it      */
}

static void check_case_and_spelling(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);
    parse(&p, "video_mode = NTSC\naspect_ratio = 16:9\nwidescreen = TRUE\n");

    assert(p.mode == ATLAS_VMODE_NTSC);
    assert(p.aspect == ATLAS_ASPECT_16_9);
    assert(p.widescreen == 1);

    atlas_profile_defaults(&p);
    parse(&p, "widescreen = off\n");
    assert(p.widescreen == 0);

    atlas_profile_defaults(&p);
    parse(&p, "video_mode = 480p\n");
    assert(p.mode == ATLAS_VMODE_480P);
}

static void check_bad_values_leave_the_field(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);

    /*
     * "perhaps" is not a boolean and "purple" is not a video mode. Both
     * must leave the field unset rather than being taken for their
     * first letter: a setting that quietly did not apply is the one
     * thing the user cannot see.
     */
    parse(&p,
          "video_mode = purple\n"
          "aspect_ratio = square\n"
          "widescreen = perhaps\n"
          "offset_x = twelve\n");

    assert(p.mode == ATLAS_VMODE_COUNT);
    assert(p.aspect == ATLAS_ASPECT_COUNT);
    assert(p.widescreen == ATLAS_PROFILE_UNSET);
    assert(p.offset_x == ATLAS_PROFILE_UNSET);
    assert(atlas_profile_is_empty(&p));
}

static void check_number_is_whole(void)
{
    atlas_profile_t p;

    /*
     * "16:9" under offset_x is a user putting a value under the wrong
     * key. strtol() would read it as 16 and shift the screen sixteen
     * pixels with nothing on screen to explain why.
     */
    atlas_profile_defaults(&p);
    parse(&p, "offset_x = 16:9\n");
    assert(p.offset_x == ATLAS_PROFILE_UNSET);

    atlas_profile_defaults(&p);
    parse(&p, "offset_y = 12px\n");
    assert(p.offset_y == ATLAS_PROFILE_UNSET);

    atlas_profile_defaults(&p);
    parse(&p, "offset_x = +7\n");
    assert(p.offset_x == 7);
}

static void check_offsets_clamp(void)
{
    atlas_profile_t p;

    /*
     * Clamped to the same limits the [video] parser uses. A file that
     * asked for 90 and read back as 32 would look like a setting that
     * changed itself between launches.
     */
    atlas_profile_defaults(&p);
    parse(&p, "offset_x = 900\noffset_y = -900\n");

    assert(p.offset_x == ATLAS_CFG_OFFSET_LIMIT);
    assert(p.offset_y == -ATLAS_CFG_OFFSET_LIMIT);
}

static void check_unknown_keys_do_not_fail_the_file(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);
    parse(&p,
          "# a profile from a later version\n"
          "[SLUS-20946]\n"
          "cheats = on\n"
          "video_mode = pal\n"
          "gsm_field_fix = 1\n");

    /* The section header is ignored and the good key still applied. */
    assert(p.mode == ATLAS_VMODE_PAL);
}

static void check_id_survives_the_parse(void)
{
    atlas_profile_t p;

    atlas_profile_defaults(&p);
    snprintf(p.id, sizeof(p.id), "SLUS-20946");

    /*
     * The ID comes from the filename. Nothing inside the file may
     * change it, or a profile claiming to be another game would apply
     * that game's settings to this one.
     */
    parse(&p, "[SCES-50000]\nvideo_mode = ntsc\n");

    assert(strcmp(p.id, "SLUS-20946") == 0);
    assert(p.mode == ATLAS_VMODE_NTSC);
}

static void check_long_launch_app_is_refused(void)
{
    atlas_profile_t p;
    char text[ATLAS_INI_VALUE_MAX + 32];
    int i, n;

    atlas_profile_defaults(&p);

    n = snprintf(text, sizeof(text), "launch_app = mc0:/");

    /*
     * Truncating a path yields a different file, and the failure would
     * arrive after AtlasPS2 has shut itself down to hand over - the one
     * point where nothing is left to report it.
     */
    for (i = 0; i < ATLAS_PROFILE_PATH_MAX; i++)
        text[n++] = 'A';

    text[n++] = '\n';
    text[n] = '\0';

    parse(&p, text);

    assert(p.launch_app[0] == '\0');
}

/* ------------------------------------------------------------------ */
/* Paths                                                               */
/* ------------------------------------------------------------------ */

static void check_paths(void)
{
    char out[192];

    assert(atlas_profile_path("mc0:/ATLAS/PROFILES", "SLUS-20946",
                              out, sizeof(out)) == ATLAS_OK);
    assert(strcmp(out, "mc0:/ATLAS/PROFILES/SLUS_20946.INI") == 0);

    /*
     * Every spelling a disc actually uses reaches the same file. The
     * underscore is how the ID appears in SYSTEM.CNF, the dash is how
     * it is written down; a profile must not depend on which one the
     * caller happened to have.
     */
    assert(atlas_profile_path("mc0:/P", "slus_20946", out, sizeof(out))
           == ATLAS_OK);
    assert(strcmp(out, "mc0:/P/SLUS_20946.INI") == 0);

    assert(atlas_profile_path("mc0:/P", "cdrom0:\\SLUS_209.46;1", out,
                              sizeof(out)) == ATLAS_OK);
    assert(strcmp(out, "mc0:/P/SLUS_20946.INI") == 0);

    /*
     * A space is not one of those spellings, and is refused rather than
     * cleaned up. The same normalizer keys the compatibility database:
     * inventing an ID out of something that was not one would match a
     * real game's entry and apply another title's settings here.
     */
    assert(atlas_profile_path("mc0:/P", "SLUS 20946", out, sizeof(out))
           == ATLAS_EFORMAT);

    /* Not a game ID at all. */
    assert(atlas_profile_path("mc0:/P", "hello", out, sizeof(out))
           == ATLAS_EFORMAT);

    /* No room: reported, never truncated into a path to another file. */
    assert(atlas_profile_path("mc0:/ATLAS/PROFILES", "SLUS-20946",
                              out, 8) == ATLAS_EINVAL);
}

/* ------------------------------------------------------------------ */
/* Formatting                                                          */
/* ------------------------------------------------------------------ */

static void check_format_omits_unset(void)
{
    atlas_profile_t p;
    char buf[ATLAS_PROFILE_FILE_MAX];
    int n;

    atlas_profile_defaults(&p);
    snprintf(p.id, sizeof(p.id), "SLUS-20946");
    p.aspect = ATLAS_ASPECT_16_9;

    n = atlas_profile_format(&p, buf, sizeof(buf));
    assert(n > 0);

    assert(strstr(buf, "aspect_ratio = 16:9") != NULL);

    /*
     * A file listing every key looks like a set of deliberate choices.
     * The next person to read it - possibly the same person a year
     * later - could not tell which ones were.
     */
    assert(strstr(buf, "video_mode") == NULL);
    assert(strstr(buf, "offset_x") == NULL);
    assert(strstr(buf, "widescreen") == NULL);
    assert(strstr(buf, "launch_app") == NULL);
}

static void check_round_trip(void)
{
    atlas_profile_t a, b;
    char buf[ATLAS_PROFILE_FILE_MAX];

    atlas_profile_defaults(&a);
    snprintf(a.id, sizeof(a.id), "SCES-50000");
    a.mode = ATLAS_VMODE_480P;
    a.aspect = ATLAS_ASPECT_4_3;
    a.offset_x = -3;
    a.offset_y = 0;
    a.widescreen = 0;
    snprintf(a.launch_app, sizeof(a.launch_app), "mass:/APPS/OPL.ELF");

    assert(atlas_profile_format(&a, buf, sizeof(buf)) > 0);

    atlas_profile_defaults(&b);
    snprintf(b.id, sizeof(b.id), "SCES-50000");
    parse(&b, buf);

    assert(b.mode == a.mode);
    assert(b.aspect == a.aspect);
    assert(b.offset_x == a.offset_x);
    assert(b.offset_y == a.offset_y);     /* 0, not lost as "unset" */
    assert(b.widescreen == a.widescreen); /* 0, likewise            */
    assert(strcmp(b.launch_app, a.launch_app) == 0);
}

static void check_format_reports_no_room(void)
{
    atlas_profile_t p;
    char small[16];

    atlas_profile_defaults(&p);
    snprintf(p.id, sizeof(p.id), "SLUS-20946");
    p.mode = ATLAS_VMODE_PAL;

    /* -1, never a length that was never written. */
    assert(atlas_profile_format(&p, small, (int)sizeof(small)) == -1);
}

/* ------------------------------------------------------------------ */

static void check_apply_video(void)
{
    atlas_profile_t p;
    atlas_video_cfg_t cfg, before;

    atlas_profile_defaults(&p);

    memset(&cfg, 0, sizeof(cfg));
    cfg.mode = ATLAS_VMODE_PAL;
    cfg.aspect = ATLAS_ASPECT_16_9;
    cfg.offset_x = 5;
    cfg.offset_y = -5;
    cfg.overscan_x = 8;
    cfg.overscan_y = 8;
    before = cfg;

    /* An empty profile changes nothing at all. */
    atlas_profile_apply_video(&p, &cfg);
    assert(memcmp(&cfg, &before, sizeof(cfg)) == 0);

    p.mode = ATLAS_VMODE_NTSC;
    atlas_profile_apply_video(&p, &cfg);

    assert(cfg.mode == ATLAS_VMODE_NTSC);
    assert(cfg.aspect == ATLAS_ASPECT_16_9);   /* still untouched */
    assert(cfg.overscan_x == 8);               /* never a profile field */
    assert(cfg.overscan_y == 8);
}

static void check_null_arguments(void)
{
    atlas_profile_t p;
    char buf[64];

    atlas_profile_defaults(&p);

    assert(atlas_profile_parse(NULL, "x", 1) == ATLAS_EINVAL);
    assert(atlas_profile_parse(&p, NULL, 1) == ATLAS_EINVAL);
    assert(atlas_profile_format(NULL, buf, sizeof(buf)) == -1);
    assert(atlas_profile_format(&p, NULL, 10) == -1);
    assert(atlas_profile_path(NULL, "SLUS-20946", buf, sizeof(buf))
           == ATLAS_EINVAL);
    assert(atlas_profile_is_empty(NULL) == 1);

    /* Must not crash on a NULL either way round. */
    atlas_profile_defaults(NULL);
    atlas_profile_apply_video(NULL, NULL);
}

int main(void)
{
    check_defaults();
    check_every_key();
    check_partial_leaves_the_rest();
    check_offset_zero_is_not_unset();
    check_case_and_spelling();
    check_bad_values_leave_the_field();
    check_number_is_whole();
    check_offsets_clamp();
    check_unknown_keys_do_not_fail_the_file();
    check_id_survives_the_parse();
    check_long_launch_app_is_refused();
    check_paths();
    check_format_omits_unset();
    check_round_trip();
    check_format_reports_no_room();
    check_apply_video();
    check_null_arguments();

    printf("test_profile: OK\n");
    return 0;
}
