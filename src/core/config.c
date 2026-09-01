/*
 * AtlasPS2 - config.c
 * ATLAS.INI: defaults, parsing and formatting.
 *
 * The half that touches devices is in config_io.c. This file is pure
 * data handling, so every clamp and every key name below is covered by
 * `make check` on the build machine.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "atlas/config.h"
#include "atlas/ini.h"

/* ------------------------------------------------------------------ */
/* Limits                                                              */
/*                                                                     */
/* Out-of-range values are clamped, never rejected. A user who typed   */
/* overscan_x=900 wanted more inset than they got; the maximum is much */
/* closer to that intent than falling back to zero would be.           */
/* ------------------------------------------------------------------ */

/* In the header: the settings screen clamps to the same numbers, and
 * two copies of a limit is one place for them to disagree. */
#define CFG_OFFSET_LIMIT   ATLAS_CFG_OFFSET_LIMIT
#define CFG_OVERSCAN_LIMIT ATLAS_CFG_OVERSCAN_LIMIT

/*
 * Longest auto-launch countdown offered. AtlasPS2 draws no clock while
 * it waits, so from the user's side a long timeout and a hang look the
 * same - and the escape from it is a button press they have to guess.
 * Thirty seconds is long enough to be deliberate and short enough that
 * waiting it out is always an option.
 *
 * Public in config.h for the same reason the trim limits are: the
 * settings screen must stop where the parser stops.
 */
#define CFG_TIMEOUT_MAX ATLAS_CFG_TIMEOUT_MAX

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void atlas_config_defaults(atlas_config_t *cfg)
{
    if (!cfg)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->lang    = ATLAS_LANG_EN;
    cfg->startup = ATLAS_STARTUP_HOME;
    snprintf(cfg->theme, sizeof(cfg->theme), "%s", "default");

    atlas_video_cfg_defaults(&cfg->video);

    cfg->animations = 1;
    cfg->sounds     = 1;

    /* No default application and no countdown: a fresh install must
     * land on the menu. Booting straight into someone else's program on
     * first run leaves a user with no way to reach the settings. */
    cfg->default_app[0] = '\0';
    cfg->timeout        = 0;
}

/* ------------------------------------------------------------------ */
/* Value helpers                                                       */
/* ------------------------------------------------------------------ */

/**
 * Read a boolean the way a person would write one.
 *
 * "1/0", "on/off", "true/false", "yes/no" all appear in configuration
 * files in the wild, and a user editing this by hand has no way to know
 * which one we accept. Anything unrecognised keeps the existing value
 * rather than guessing.
 */
static int parse_bool(const char *v, int fallback)
{
    if (!v || !v[0])
        return fallback;

    if (strcmp(v, "1") == 0 || strcmp(v, "on") == 0
        || strcmp(v, "true") == 0 || strcmp(v, "yes") == 0)
        return 1;

    if (strcmp(v, "0") == 0 || strcmp(v, "off") == 0
        || strcmp(v, "false") == 0 || strcmp(v, "no") == 0)
        return 0;

    return fallback;
}

/**
 * Read an integer, clamped into [lo, hi].
 *
 * Text that is not a number at all keeps `fallback`: atoi() would read
 * "left" as 0, which is a legal offset, so a typo would silently become
 * a setting the user never chose.
 */
static int parse_int(const char *v, int lo, int hi, int fallback)
{
    const char *p = v;
    int n;

    if (!v || !v[0])
        return fallback;

    if (*p == '-' || *p == '+')
        p++;

    if (*p == '\0')
        return fallback;

    for (; *p; p++) {
        if (*p < '0' || *p > '9')
            return fallback;
    }

    n = atoi(v);
    return ATLAS_CLAMP(n, lo, hi);
}

/* ------------------------------------------------------------------ */
/* Key mapping                                                         */
/* ------------------------------------------------------------------ */

int atlas_config_set(atlas_config_t *cfg, const char *section,
                     const char *key, const char *value)
{
    if (!cfg || !section || !key || !value)
        return 0;

    if (strcmp(section, "system") == 0) {
        if (strcmp(key, "language") == 0) {
            cfg->lang = atlas_i18n_lang_from_code(value);
            return 1;
        }
        if (strcmp(key, "theme") == 0) {
            snprintf(cfg->theme, sizeof(cfg->theme), "%s", value);
            return 1;
        }
        if (strcmp(key, "startup") == 0) {
            cfg->startup = (strcmp(value, "apps") == 0)
                         ? ATLAS_STARTUP_APPS : ATLAS_STARTUP_HOME;
            return 1;
        }
        return 0;
    }

    if (strcmp(section, "video") == 0) {
        if (strcmp(key, "mode") == 0) {
            cfg->video.mode = atlas_video_mode_from_label(value);
            return 1;
        }
        if (strcmp(key, "aspect") == 0) {
            cfg->video.aspect = atlas_video_aspect_from_label(value);
            return 1;
        }
        if (strcmp(key, "offset_x") == 0) {
            cfg->video.offset_x = parse_int(value, -CFG_OFFSET_LIMIT,
                                            CFG_OFFSET_LIMIT,
                                            cfg->video.offset_x);
            return 1;
        }
        if (strcmp(key, "offset_y") == 0) {
            cfg->video.offset_y = parse_int(value, -CFG_OFFSET_LIMIT,
                                            CFG_OFFSET_LIMIT,
                                            cfg->video.offset_y);
            return 1;
        }
        if (strcmp(key, "overscan_x") == 0) {
            cfg->video.overscan_x = parse_int(value, 0, CFG_OVERSCAN_LIMIT,
                                              cfg->video.overscan_x);
            return 1;
        }
        if (strcmp(key, "overscan_y") == 0) {
            cfg->video.overscan_y = parse_int(value, 0, CFG_OVERSCAN_LIMIT,
                                              cfg->video.overscan_y);
            return 1;
        }
        return 0;
    }

    if (strcmp(section, "ui") == 0) {
        if (strcmp(key, "animations") == 0) {
            cfg->animations = parse_bool(value, cfg->animations);
            return 1;
        }
        if (strcmp(key, "sounds") == 0) {
            cfg->sounds = parse_bool(value, cfg->sounds);
            return 1;
        }
        return 0;
    }

    if (strcmp(section, "boot") == 0) {
        if (strcmp(key, "default_app") == 0) {
            /*
             * A path too long to hold whole is dropped, not truncated.
             * A shortened path names a different file, and this one is
             * launched without the user asking.
             */
            if ((int)strlen(value) < ATLAS_CFG_PATH_MAX)
                snprintf(cfg->default_app, sizeof(cfg->default_app),
                         "%s", value);
            else
                cfg->default_app[0] = '\0';
            return 1;
        }
        if (strcmp(key, "timeout") == 0) {
            cfg->timeout = parse_int(value, 0, CFG_TIMEOUT_MAX,
                                     cfg->timeout);
            return 1;
        }
        return 0;
    }

    /* Unknown section: ignored, per the spec. A file written by a later
     * version must still load in this one. */
    return 0;
}

/* ------------------------------------------------------------------ */
/* Parsing                                                             */
/* ------------------------------------------------------------------ */

static int cfg_cb(void *user, const char *section, const char *key,
                  const char *value)
{
    atlas_config_set((atlas_config_t *)user, section, key, value);
    return 0;
}

void atlas_config_parse(atlas_config_t *cfg, const char *text, int len,
                        int *bad_lines)
{
    if (bad_lines)
        *bad_lines = 0;

    if (!cfg)
        return;

    /* Defaults first, so every field the file omits still has a value.
     * A partial file must produce a whole configuration. */
    atlas_config_defaults(cfg);

    if (!text || len <= 0)
        return;

    atlas_ini_parse(text, len, cfg_cb, cfg, bad_lines);
}

/* ------------------------------------------------------------------ */
/* Formatting                                                          */
/*                                                                     */
/* The comments are not decoration. This file is what a user opens on  */
/* a PC when the console will not display, so it has to explain its    */
/* own keys without a manual to hand.                                  */
/* ------------------------------------------------------------------ */

int atlas_config_format(const atlas_config_t *cfg, char *out, int size)
{
    int n;

    if (!cfg || !out || size <= 0)
        return -1;

    n = snprintf(out, (size_t)size,
        "# " ATLAS_NAME " " ATLAS_VERSION_STRING " configuration\n"
        "#\n"
        "# Edit with any text editor. Unknown keys are ignored, and a\n"
        "# damaged file falls back to ATLAS.INI.BAK beside it.\n"
        "\n"
        "[system]\n"
        "# language: en or fr\n"
        "language=%s\n"
        "theme=%s\n"
        "# startup: home or apps\n"
        "startup=%s\n"
        "\n"
        "[video]\n"
        "# mode: auto, ntsc, pal, 480p\n"
        "#   480p needs a component or VGA cable. If the screen goes\n"
        "#   blank, hold R1 while starting to boot in safe video mode.\n"
        "mode=%s\n"
        "# aspect: auto, 4:3, 16:9\n"
        "aspect=%s\n"
        "# Screen position trim, -%d to %d pixels.\n"
        "offset_x=%d\n"
        "offset_y=%d\n"
        "# Extra inset from the screen edges, 0 to %d pixels.\n"
        "overscan_x=%d\n"
        "overscan_y=%d\n"
        "\n"
        "[ui]\n"
        "animations=%d\n"
        "sounds=%d\n"
        "\n"
        "[boot]\n"
        "# default_app: full path to an .ELF, or blank for the menu.\n"
        "default_app=%s\n"
        "# timeout: seconds before default_app starts, 0 to disable\n"
        "#   (maximum %d).\n"
        "timeout=%d\n",
        atlas_i18n_lang_code(cfg->lang),
        cfg->theme[0] ? cfg->theme : "default",
        (cfg->startup == ATLAS_STARTUP_APPS) ? "apps" : "home",
        atlas_video_mode_label(cfg->video.mode),
        atlas_video_aspect_label(cfg->video.aspect),
        CFG_OFFSET_LIMIT, CFG_OFFSET_LIMIT,
        cfg->video.offset_x,
        cfg->video.offset_y,
        CFG_OVERSCAN_LIMIT,
        cfg->video.overscan_x,
        cfg->video.overscan_y,
        cfg->animations ? 1 : 0,
        cfg->sounds ? 1 : 0,
        cfg->default_app,
        CFG_TIMEOUT_MAX,
        cfg->timeout);

    /*
     * snprintf reports what it WOULD have written. A result that did
     * not fit is a truncated INI - one that can end mid-value and parse
     * back as something else - so it is refused rather than returned.
     */
    if (n < 0 || n >= size)
        return -1;

    return n;
}
