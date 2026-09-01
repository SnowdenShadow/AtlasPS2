/*
 * AtlasPS2 - profile.c
 * Per-title display settings: parsing, formatting and path building.
 *
 * No fileXio here, for the reason theme.c and compat.c are split the
 * same way: what a setting means is checkable on the build machine, and
 * a wrong answer is invisible until a television is in front of you.
 * The read and the write are in profile_io.c.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/profile.h"
#include "atlas/config.h"
#include "atlas/ini.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Values                                                              */
/* ------------------------------------------------------------------ */

/** ASCII case-insensitive compare, as compat.c does it and why. */
static int ieq(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] && b[i]; i++) {
        char ca = a[i], cb = b[i];

        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');

        if (ca != cb)
            return 0;
    }

    return a[i] == b[i];
}

/**
 * Read a boolean the way a person writes one.
 *
 * The same spellings compat.c accepts, matched whole for the same
 * reason: a value taken for its first letter turns "nearly" into "no"
 * and reports nothing.
 */
static int parse_bool(const char *v, int *out)
{
    static const char *const k_true[]  = { "1", "yes", "true", "on" };
    static const char *const k_false[] = { "0", "no", "false", "off" };
    int i;

    for (i = 0; i < ATLAS_ARRAY_COUNT(k_true); i++) {
        if (ieq(v, k_true[i])) { *out = 1; return 0; }
    }

    for (i = 0; i < ATLAS_ARRAY_COUNT(k_false); i++) {
        if (ieq(v, k_false[i])) { *out = 0; return 0; }
    }

    return -1;
}

/**
 * Read a signed decimal, whole.
 *
 * strtol() would accept "16:9" as 16, which matters here: aspect_ratio
 * and offset_x sit in the same file and a user can put a value under
 * the wrong key. Refusing trailing junk turns that into a logged line
 * instead of a screen shifted sixteen pixels for no visible reason.
 */
static int parse_int(const char *v, int *out)
{
    int sign = 1, n = 0, digits = 0, i = 0;

    if (v[i] == '+' || v[i] == '-') {
        sign = (v[i] == '-') ? -1 : 1;
        i++;
    }

    for (; v[i] >= '0' && v[i] <= '9'; i++) {
        n = n * 10 + (v[i] - '0');
        digits++;

        if (n > 100000)         /* far past any field here */
            return -1;
    }

    if (!digits || v[i] != '\0')
        return -1;

    *out = n * sign;
    return 0;
}

static const struct { const char *name; atlas_vmode_t mode; } k_mode[] = {
    { "auto", ATLAS_VMODE_AUTO },
    { "ntsc", ATLAS_VMODE_NTSC },
    { "pal",  ATLAS_VMODE_PAL  },
    { "480p", ATLAS_VMODE_480P }
};

static const struct { const char *name; atlas_aspect_t aspect; } k_aspect[] = {
    { "auto", ATLAS_ASPECT_AUTO },
    { "4:3",  ATLAS_ASPECT_4_3  },
    { "16:9", ATLAS_ASPECT_16_9 }
};

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void atlas_profile_defaults(atlas_profile_t *p)
{
    if (!p)
        return;

    memset(p, 0, sizeof(*p));

    /*
     * COUNT is the unset marker for the two enums, because their zero
     * value is AUTO and AUTO is a real choice a user can make. A
     * profile saying "auto" must override a global setting of PAL; a
     * profile that says nothing must not.
     */
    p->mode       = ATLAS_VMODE_COUNT;
    p->aspect     = ATLAS_ASPECT_COUNT;
    p->offset_x   = ATLAS_PROFILE_UNSET;
    p->offset_y   = ATLAS_PROFILE_UNSET;
    p->widescreen = ATLAS_PROFILE_UNSET;
}

int atlas_profile_is_empty(const atlas_profile_t *p)
{
    if (!p)
        return 1;

    return p->mode == ATLAS_VMODE_COUNT
        && p->aspect == ATLAS_ASPECT_COUNT
        && p->offset_x == ATLAS_PROFILE_UNSET
        && p->offset_y == ATLAS_PROFILE_UNSET
        && p->widescreen == ATLAS_PROFILE_UNSET
        && p->launch_app[0] == '\0';
}

/* ------------------------------------------------------------------ */
/* Parsing                                                             */
/* ------------------------------------------------------------------ */

static int profile_key(void *user, const char *section, const char *key,
                       const char *value)
{
    atlas_profile_t *p = (atlas_profile_t *)user;
    int i, n;

    /*
     * Sections are ignored, not rejected. The file is named after the
     * game, so a [SLUS-20946] header in it is redundant - but people
     * write one, having seen the compatibility file, and a parser that
     * treated its keys as belonging to nothing would leave that user
     * with a profile that silently does nothing at all.
     */
    ATLAS_UNUSED(section);

    if (strcmp(key, "video_mode") == 0) {
        for (i = 0; i < ATLAS_ARRAY_COUNT(k_mode); i++) {
            if (ieq(value, k_mode[i].name)) {
                p->mode = k_mode[i].mode;
                return 0;
            }
        }

        ATLAS_LOG("PROFILE", "video_mode='%s' unknown", value);
        return 0;
    }

    if (strcmp(key, "aspect_ratio") == 0) {
        for (i = 0; i < ATLAS_ARRAY_COUNT(k_aspect); i++) {
            if (ieq(value, k_aspect[i].name)) {
                p->aspect = k_aspect[i].aspect;
                return 0;
            }
        }

        ATLAS_LOG("PROFILE", "aspect_ratio='%s' unknown", value);
        return 0;
    }

    if (strcmp(key, "offset_x") == 0 || strcmp(key, "offset_y") == 0) {
        if (parse_int(value, &n) != 0) {
            ATLAS_LOG("PROFILE", "%s='%s' is not a number", key, value);
            return 0;
        }

        /*
         * Clamped where the [video] section is clamped, and to the same
         * numbers. A profile allowed to ask for 90 would write a file
         * that reads back as 32, and the setting would appear to change
         * on its own the next time the game was launched.
         */
        n = ATLAS_CLAMP(n, -ATLAS_CFG_OFFSET_LIMIT, ATLAS_CFG_OFFSET_LIMIT);

        if (key[7] == 'x')
            p->offset_x = n;
        else
            p->offset_y = n;

        return 0;
    }

    if (strcmp(key, "widescreen") == 0) {
        if (parse_bool(value, &n) != 0) {
            ATLAS_LOG("PROFILE", "widescreen='%s' is not yes or no", value);
            return 0;
        }

        p->widescreen = n;
        return 0;
    }

    if (strcmp(key, "launch_app") == 0) {
        if ((int)strlen(value) >= ATLAS_PROFILE_PATH_MAX) {
            /*
             * Refused rather than truncated: a truncated path is a
             * different file, and the failure would arrive at the point
             * of no return, after AtlasPS2 has already shut down.
             */
            ATLAS_LOG("PROFILE", "launch_app is too long; ignored");
            return 0;
        }

        snprintf(p->launch_app, sizeof(p->launch_app), "%s", value);
        return 0;
    }

    /*
     * An unknown key is skipped and logged. A profile written by a
     * later version - or shared by someone using a different launcher -
     * should still apply the settings this build does understand.
     */
    ATLAS_LOG("PROFILE", "unknown key '%s'", key);
    return 0;
}

atlas_err_t atlas_profile_parse(atlas_profile_t *p, const char *text, int len)
{
    char id[ATLAS_DISC_ID_MAX];

    if (!p || !text)
        return ATLAS_EINVAL;

    /*
     * The ID survives the parse. It comes from the filename, which is
     * how the profile was found in the first place, and nothing in the
     * file is allowed to change it: a file named SLUS_20946.INI that
     * claimed to be a different game would apply someone else's
     * settings to this one.
     */
    snprintf(id, sizeof(id), "%s", p->id);

    atlas_profile_defaults(p);
    snprintf(p->id, sizeof(p->id), "%s", id);

    return atlas_ini_parse(text, len, profile_key, p, NULL);
}

/* ------------------------------------------------------------------ */
/* Formatting                                                          */
/* ------------------------------------------------------------------ */

int atlas_profile_format(const atlas_profile_t *p, char *out, int size)
{
    int n = 0;
    int i;

    if (!p || !out || size <= 0)
        return -1;

    /*
     * snprintf returns what it WOULD have written, so n can run past
     * size. Every step below checks before writing, and the final
     * comparison catches the overflow rather than reporting a length
     * that was never stored.
     */
    n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                  "# AtlasPS2 profile for %s\n"
                  "# Every key is optional; a key left out is left alone.\n",
                  p->id[0] ? p->id : "this title");

    if (p->mode != ATLAS_VMODE_COUNT) {
        for (i = 0; i < ATLAS_ARRAY_COUNT(k_mode); i++) {
            if (k_mode[i].mode == p->mode) {
                n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                              "video_mode = %s\n", k_mode[i].name);
                break;
            }
        }
    }

    if (p->aspect != ATLAS_ASPECT_COUNT) {
        for (i = 0; i < ATLAS_ARRAY_COUNT(k_aspect); i++) {
            if (k_aspect[i].aspect == p->aspect) {
                n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                              "aspect_ratio = %s\n", k_aspect[i].name);
                break;
            }
        }
    }

    if (p->offset_x != ATLAS_PROFILE_UNSET)
        n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                      "offset_x = %d\n", p->offset_x);

    if (p->offset_y != ATLAS_PROFILE_UNSET)
        n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                      "offset_y = %d\n", p->offset_y);

    if (p->widescreen != ATLAS_PROFILE_UNSET)
        n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                      "widescreen = %s\n", p->widescreen ? "yes" : "no");

    if (p->launch_app[0])
        n += snprintf(out + n, (n < size) ? (size_t)(size - n) : 0,
                      "launch_app = %s\n", p->launch_app);

    if (n >= size)
        return -1;

    return n;
}

/* ------------------------------------------------------------------ */
/* Paths                                                               */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_profile_path(const char *dir, const char *id,
                               char *out, int size)
{
    char norm[ATLAS_DISC_ID_MAX];
    char name[ATLAS_DISC_ID_MAX + 4];
    int i, j = 0;
    int len;

    if (!dir || !id || !out || size <= 0)
        return ATLAS_EINVAL;

    /*
     * Normalised first, so that "slus 20946", "SLUS_20946" and
     * "SLUS-20946" all reach the same file. Otherwise a user who typed
     * the ID one way would get a second profile for a game that already
     * had one, and neither would appear to work reliably.
     */
    if (atlas_disc_id_normalize(id, norm, sizeof(norm)) != ATLAS_OK)
        return ATLAS_EFORMAT;

    /*
     * The dash becomes an underscore. Memory Card filenames go through
     * mcman, which is conservative about punctuation, and an
     * underscore is what every other PS2 tool writes these as - so a
     * profile copied from a PC lands on the name AtlasPS2 looks for.
     */
    for (i = 0; norm[i] && j < (int)sizeof(name) - 5; i++)
        name[j++] = (norm[i] == '-') ? '_' : norm[i];

    name[j] = '\0';

    len = snprintf(out, (size_t)size, "%s/%s.INI", dir, name);

    if (len < 0 || len >= size)
        return ATLAS_EINVAL;

    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* Applying                                                            */
/* ------------------------------------------------------------------ */

void atlas_profile_apply_video(const atlas_profile_t *p,
                               atlas_video_cfg_t *cfg)
{
    if (!p || !cfg)
        return;

    if (p->mode != ATLAS_VMODE_COUNT)
        cfg->mode = p->mode;

    if (p->aspect != ATLAS_ASPECT_COUNT)
        cfg->aspect = p->aspect;

    if (p->offset_x != ATLAS_PROFILE_UNSET)
        cfg->offset_x = p->offset_x;

    if (p->offset_y != ATLAS_PROFILE_UNSET)
        cfg->offset_y = p->offset_y;

    /*
     * Overscan is deliberately not a profile field. It trims the safe
     * area AtlasPS2 draws inside, and the program AtlasPS2 launches
     * draws its own - so a per-game overscan would be a setting with no
     * effect on the thing the user was looking at when they set it.
     */
}
