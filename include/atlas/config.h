/*
 * AtlasPS2 - config.h
 *
 * ATLAS.INI: everything the user chose, in one editable text file.
 *
 * WHY A TEXT FILE
 * ---------------
 * A binary blob is smaller and faster to parse, and it is also
 * unrecoverable by the person holding the console. When the settings
 * are what stops AtlasPS2 from booting - a video mode this TV cannot
 * show is the obvious one - the fix has to be something a user can do
 * with the Memory Card in a card reader and Notepad open. So the file
 * is INI, the keys are words, and anything the parser does not
 * understand is ignored rather than rejected.
 *
 * WHY LOADING NEVER FAILS
 * -----------------------
 * atlas_config_load() always leaves a usable configuration behind.
 * A missing file, an unreadable device, a file full of nonsense: each
 * one ends with the safe defaults and a note in the result about what
 * happened. There is no path through this module that hands the caller
 * a half-populated struct, because the caller is boot and it has
 * nothing to fall back to.
 */
#ifndef ATLAS_CONFIG_H
#define ATLAS_CONFIG_H

#include "atlas/atlas.h"
#include "atlas/video.h"
#include "atlas/i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Where the cursor starts on the home screen. */
typedef enum {
    ATLAS_STARTUP_HOME = 0,
    ATLAS_STARTUP_APPS,
    ATLAS_STARTUP_COUNT
} atlas_startup_t;

#define ATLAS_CFG_THEME_MAX 32
#define ATLAS_CFG_PATH_MAX  128

/*
 * The range the [video] trim settings are clamped to on load.
 *
 * Public because the settings screen has to stop at the same numbers
 * the parser does. A screen that let a user pick 90 would write a file
 * that reads back as 64, and the setting would change on its own the
 * next time the console started.
 */
#define ATLAS_CFG_OFFSET_LIMIT   32
#define ATLAS_CFG_OVERSCAN_LIMIT 64

/*
 * Longest auto-launch countdown the parser will keep, and so the
 * highest the settings screen may offer. AtlasPS2 draws no clock while
 * it waits, so from the user's side a long timeout and a hang look the
 * same.
 */
#define ATLAS_CFG_TIMEOUT_MAX 30

/** The whole of ATLAS.INI, as a struct. */
typedef struct {
    /* [system] */
    atlas_lang_t    lang;
    char            theme[ATLAS_CFG_THEME_MAX];
    atlas_startup_t startup;

    /* [video] */
    atlas_video_cfg_t video;

    /* [ui] */
    int animations;   /* 0/1 */
    int sounds;       /* 0/1 */

    /* [boot] */
    char default_app[ATLAS_CFG_PATH_MAX];

    /**
     * Seconds to wait before launching `default_app`, or 0 for never.
     * Capped on load: an hour-long countdown on a console with no clock
     * on screen is indistinguishable from a hang.
     */
    int timeout;
} atlas_config_t;

/** The longest ATLAS.INI this module will read or write. */
#define ATLAS_CFG_FILE_MAX 2048

/** What a load actually did. Useful to the UI and to Recovery. */
typedef enum {
    ATLAS_CFG_DEFAULTS = 0, /**< no file found; defaults in use        */
    ATLAS_CFG_LOADED,       /**< the main file parsed cleanly          */
    ATLAS_CFG_RECOVERED,    /**< main file was damaged; .BAK was used  */
    ATLAS_CFG_PARTIAL       /**< parsed, but some lines were unusable  */
} atlas_config_origin_t;

/** Fill `cfg` with the safe defaults. Never fails. */
void atlas_config_defaults(atlas_config_t *cfg);

/**
 * Apply one key/value pair, as the INI reader delivers it.
 *
 * Exposed so the self-checks can drive the whole mapping without a
 * filesystem, and so a future settings screen can reuse the same
 * validation instead of writing a second copy of the clamps.
 *
 * @return 1 if the key was recognised, 0 if it was ignored.
 */
int atlas_config_set(atlas_config_t *cfg, const char *section,
                     const char *key, const char *value);

/**
 * Parse a buffer into `cfg`, starting from the defaults.
 *
 * Values out of range are clamped to something usable, not rejected:
 * an overscan of 900 means the user wanted more inset, and giving them
 * the maximum is closer to their intent than giving them zero.
 *
 * @param bad_lines  optional; lines the reader could not use.
 */
void atlas_config_parse(atlas_config_t *cfg, const char *text, int len,
                        int *bad_lines);

/**
 * Render `cfg` as the text of an ATLAS.INI, comments included.
 *
 * The comments are the point: this file is the one a user opens to fix
 * a console that will not display, so it documents its own keys.
 *
 * @return bytes written, or -1 if `size` was too small.
 */
int atlas_config_format(const atlas_config_t *cfg, char *out, int size);

/**
 * Load the configuration from the first device that has one.
 *
 * Tries mc0, then mc1, then USB. A file whose parse produced too much
 * garbage to trust is set aside and its `.BAK` is tried instead - that
 * is the recovery path the spec asks for, and it is why saving keeps a
 * backup at all.
 *
 * Always leaves `cfg` usable.
 *
 * @param origin  optional; how the result was arrived at.
 */
void atlas_config_load(atlas_config_t *cfg, atlas_config_origin_t *origin);

/**
 * Write the configuration back to the device it should live on.
 *
 * Writes through atlas_file_write_atomic(), so the previous file
 * becomes ATLAS.INI.BAK and there is no moment at which the card holds
 * no complete copy. Creates the folder if needed.
 *
 * @return ATLAS_OK, ATLAS_ENODEV if no device could take it, or the
 *         underlying I/O error.
 */
atlas_err_t atlas_config_save(const atlas_config_t *cfg);

/** The path the last successful load came from, or "" if none. */
const char *atlas_config_source(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_CONFIG_H */
