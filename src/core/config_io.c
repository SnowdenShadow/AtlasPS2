/*
 * AtlasPS2 - config_io.c
 * Finding, reading and writing ATLAS.INI, and the language files.
 *
 * EE-only: this is the half that touches devices. The parsing it drives
 * lives in config.c and i18n.c, both of which are host-tested.
 */
#include <string.h>
#include <stdio.h>

#include "atlas/config.h"
#include "atlas/device.h"
#include "atlas/file.h"
#include "atlas/ini.h"
#include "atlas/i18n.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Where the configuration lives                                       */
/*                                                                     */
/* Memory Card first, USB last. A card is the device that stays in the */
/* console, so it is where settings belong; a stick that happens to be */
/* plugged in should not quietly take over the configuration of a      */
/* console it was borrowed by.                                         */
/* ------------------------------------------------------------------ */

#define CFG_DIR  "ATLAS/CONFIG"
#define CFG_FILE "ATLAS/CONFIG/ATLAS.INI"

static const atlas_device_id_t s_search[] = {
    ATLAS_DEV_MC0,
    ATLAS_DEV_MC1,
    ATLAS_DEV_MASS
};

/*
 * How much garbage makes a file untrustworthy.
 *
 * A couple of bad lines is a user's typo, and the rest of the file is
 * still their settings - falling back to a backup there would throw
 * away good values to fix a bad one. A file that is mostly unreadable
 * is a different thing: a partial write, or a card going bad. The
 * threshold sits between the two.
 */
#define CFG_BAD_LINE_LIMIT 8

/* Long enough for "mass:/ATLAS/CONFIG/ATLAS.INI" several times over;
 * a device path that does not fit is skipped rather than shortened. */
#define CFG_PATH_MAX 160

/* A path plus the ".BAK" suffix. Everything that can hold either one
 * is sized from this, so the suffix is never what gets cut off. */
#define CFG_BAK_MAX (CFG_PATH_MAX + 8)

/* Sized for the backup path too: this is reported to the user as where
 * their settings came from, and "recovered from the backup" is exactly
 * the case where the suffix is the informative part. */
static char s_source[CFG_BAK_MAX];

const char *atlas_config_source(void)
{
    return s_source;
}

/**
 * Read and parse one candidate file.
 *
 * @return 1 if the file was read and looked trustworthy, 0 otherwise.
 */
static int try_file(const char *path, atlas_config_t *cfg, int *bad_out)
{
    char buf[ATLAS_CFG_FILE_MAX];
    int len = 0, bad = 0;
    atlas_err_t err;

    err = atlas_file_read(path, buf, (int)sizeof(buf), &len);

    /*
     * EFORMAT means it did not fit. That is not a file this build
     * wrote, and parsing the first 2 KB of it would produce settings
     * from half a document.
     */
    if (err != ATLAS_OK || len <= 0)
        return 0;

    atlas_config_parse(cfg, buf, len, &bad);

    if (bad_out)
        *bad_out = bad;

    if (bad > CFG_BAD_LINE_LIMIT) {
        ATLAS_LOG("CFG", "%s has %d bad lines, not trusted", path, bad);
        return 0;
    }

    return 1;
}

void atlas_config_load(atlas_config_t *cfg, atlas_config_origin_t *origin)
{
    int i;

    if (!cfg)
        return;

    /* Usable before anything is read, so every early return below still
     * leaves the caller with a complete configuration. */
    atlas_config_defaults(cfg);
    s_source[0] = '\0';

    if (origin)
        *origin = ATLAS_CFG_DEFAULTS;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search); i++) {
        /* The backup buffer is the larger one on purpose: it holds the
         * same path plus ".BAK", so sizing them alike would make the
         * suffix the thing that gets cut off. */
        char path[CFG_PATH_MAX];
        char bak[CFG_BAK_MAX];
        int bad = 0;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (atlas_device_path(s_search[i], CFG_FILE, path, sizeof(path))
            != ATLAS_OK)
            continue;

        if (try_file(path, cfg, &bad)) {
            snprintf(s_source, sizeof(s_source), "%s", path);
            if (origin)
                *origin = bad > 0 ? ATLAS_CFG_PARTIAL : ATLAS_CFG_LOADED;
            ATLAS_LOG("CFG", "loaded %s (%d bad lines)", path, bad);
            return;
        }

        /*
         * The main file was missing or too damaged. This is what the
         * backup exists for - and the backup is a previous version of
         * the user's own settings, which beats defaults by a long way.
         */
        snprintf(bak, sizeof(bak), "%s.BAK", path);

        if (try_file(bak, cfg, NULL)) {
            snprintf(s_source, sizeof(s_source), "%s", bak);
            if (origin)
                *origin = ATLAS_CFG_RECOVERED;
            ATLAS_LOG("CFG", "recovered from %s", bak);
            return;
        }

        /* Nothing usable here; the next device may have a copy. The
         * parse attempt may have left values behind, so reset. */
        atlas_config_defaults(cfg);
    }

    ATLAS_LOG("CFG", "no configuration found, using defaults");
}

atlas_err_t atlas_config_save(const atlas_config_t *cfg)
{
    char text[ATLAS_CFG_FILE_MAX];
    int len, i;

    if (!cfg)
        return ATLAS_EINVAL;

    len = atlas_config_format(cfg, text, (int)sizeof(text));
    if (len < 0)
        return ATLAS_EFORMAT;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search); i++) {
        char dir[CFG_PATH_MAX];
        char path[CFG_PATH_MAX];
        atlas_err_t err;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (atlas_device_path(s_search[i], CFG_DIR, dir, sizeof(dir))
            != ATLAS_OK)
            continue;

        if (atlas_device_path(s_search[i], CFG_FILE, path, sizeof(path))
            != ATLAS_OK)
            continue;

        atlas_file_mkdir_p(dir);

        err = atlas_file_write_atomic(path, text, len);
        if (err == ATLAS_OK) {
            snprintf(s_source, sizeof(s_source), "%s", path);
            ATLAS_LOG("CFG", "saved %s (%d bytes)", path, len);
            return ATLAS_OK;
        }

        /* A full or failing card should not stop the next device from
         * being tried: the point is that the settings survive. */
        ATLAS_LOG("CFG", "could not write %s (%d)", path, (int)err);
    }

    return ATLAS_ENODEV;
}

/* ------------------------------------------------------------------ */
/* Language files                                                      */
/* ------------------------------------------------------------------ */

/* A file with every key in it is a few kilobytes; this is the ceiling
 * for the whole document, matching the configuration reader. */
#define LANG_FILE_MAX 8192

static int lang_cb(void *user, const char *section, const char *key,
                   const char *value)
{
    char full[ATLAS_INI_SECTION_MAX + ATLAS_INI_KEY_MAX + 2];
    int *count = (int *)user;

    /*
     * Keys in the file are written whole - "home.games" - and the INI
     * reader splits on '.' only if the author used sections. Both
     * spellings work: a sectioned file yields section "home", key
     * "games", which we rejoin here.
     */
    if (section[0] != '\0')
        snprintf(full, sizeof(full), "%s.%s", section, key);
    else
        snprintf(full, sizeof(full), "%s", key);

    if (atlas_i18n_set(full, value))
        (*count)++;

    return 0;
}

void atlas_i18n_load_overrides(void)
{
    static char buf[LANG_FILE_MAX];
    char rel[64];
    int i;

    snprintf(rel, sizeof(rel), "ATLAS/LANG/%s.ini",
             atlas_i18n_lang_code(atlas_i18n_lang()));

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search); i++) {
        char path[CFG_PATH_MAX];
        int len = 0, applied = 0;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (atlas_device_path(s_search[i], rel, path, sizeof(path))
            != ATLAS_OK)
            continue;

        if (atlas_file_read(path, buf, (int)sizeof(buf), &len) != ATLAS_OK
            || len <= 0)
            continue;

        atlas_ini_parse(buf, len, lang_cb, &applied, NULL);

        ATLAS_LOG("I18N", "%s: %d string(s) overridden", path, applied);
        return;
    }
}
