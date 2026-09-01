/*
 * AtlasPS2 - screen_settings.c
 * Everything the user can change, in one list.
 *
 * WHY THIS SCREEN OWNS SO LITTLE
 * ------------------------------
 * Most of what a settings menu offers already exists as a screen of its
 * own: video has a confirmation countdown it cannot work without, the
 * theme screen previews live, devices and applications are lists. So
 * the rows for those are doors, not copies. What is edited here is only
 * what has no home elsewhere - the language, the startup screen, the
 * two cosmetic switches, and the two boot keys.
 *
 * The alternative, re-implementing the video controls inline, would put
 * a second copy of the clamps and the revert timer in the tree. Two
 * copies of a limit is one place for them to disagree, and the one that
 * disagrees here leaves a television showing nothing.
 *
 * WHY SAVING TOUCHES ONLY ITS OWN KEYS
 * ------------------------------------
 * Save reads ATLAS.INI, replaces the six fields this screen edits, and
 * writes it back. The video block and the theme name are left exactly
 * as the file has them, because the user may have set them from the
 * screens that own them - possibly after opening this one. A blanket
 * write of a struct captured on entry would silently undo that.
 *
 * WHY THE LANGUAGE APPLIES BEFORE IT IS SAVED
 * -------------------------------------------
 * A language you cannot read is chosen by looking at the result, not by
 * reading the label. So Left and Right switch it immediately, and
 * leaving without saving puts the previous one back - the same promise
 * the video screen's countdown makes, kept the same way.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"

#include "atlas/app.h"
#include "atlas/atlas.h"
#include "atlas/boot.h"
#include "atlas/config.h"
#include "atlas/fav.h"
#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/log.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

/*
 * The list scrolls rather than paginates, for the same reason the
 * application list does: Down past the bottom must give the next row,
 * not a new page starting on the one already selected.
 */
#define SET_VISIBLE 8

/* ------------------------------------------------------------------ */
/* Rows                                                                */
/*                                                                     */
/* The section order is the one the specification lists, and the        */
/* headings are real rows rather than decorations drawn between them:   */
/* one array means the cursor, the scrolling and the drawing all agree  */
/* about what row 12 is. Headings and information rows are skipped by   */
/* the cursor, which is the only thing that makes them different.       */
/* ------------------------------------------------------------------ */

typedef enum {
    K_HEAD = 0,   /**< a section title; never selectable                */
    K_INFO,       /**< a fact with a value; never selectable            */
    K_VALUE,      /**< Left/Right change it                             */
    K_ACTION      /**< Cross does something                             */
} set_kind_t;

typedef enum {
    R_H_GENERAL = 0,
    R_STARTUP,
    R_ANIM,
    R_SOUNDS,

    R_H_DISPLAY,
    R_VIDEO,
    R_THEME,

    R_H_DEVICES,
    R_DEVICES,
    R_USB_RETRY,

    R_H_APPS,
    R_APPS,
    R_CLEAR,

    R_H_BOOT,
    R_DEFAULT_APP,
    R_TIMEOUT,

    R_H_LANG,
    R_LANG,

    R_H_SYSTEM,
    R_SYSINFO,
    R_CFG_FILE,

    R_H_ADVANCED,
    R_RESET,

    R_H_RECOVERY,
    R_RECOVERY,

    R_H_ABOUT,
    R_ABOUT_VER,
    R_ABOUT_LICENSE,

    R_SAVE,
    R_COUNT
} set_row_t;

typedef struct {
    set_row_t      row;
    set_kind_t     kind;
    atlas_str_id_t label;

    /** The dim line under the list. ATLAS_STR_COUNT for nothing. */
    atlas_str_id_t detail;
} set_entry_t;

static const set_entry_t s_entries[R_COUNT] = {
    { R_H_GENERAL,   K_HEAD,   ATLAS_STR_SET_H_GENERAL,   ATLAS_STR_COUNT },
    { R_STARTUP,     K_VALUE,  ATLAS_STR_SET_STARTUP,     ATLAS_STR_SET_D_STARTUP },
    { R_ANIM,        K_VALUE,  ATLAS_STR_SET_ANIM,        ATLAS_STR_SET_D_ANIM },
    { R_SOUNDS,      K_VALUE,  ATLAS_STR_SET_SOUNDS,      ATLAS_STR_SET_D_SOUNDS },

    { R_H_DISPLAY,   K_HEAD,   ATLAS_STR_SET_H_DISPLAY,   ATLAS_STR_COUNT },
    { R_VIDEO,       K_ACTION, ATLAS_STR_SET_VIDEO,       ATLAS_STR_SET_D_VIDEO },
    { R_THEME,       K_ACTION, ATLAS_STR_THEME_TITLE,     ATLAS_STR_SET_D_THEME },

    { R_H_DEVICES,   K_HEAD,   ATLAS_STR_SET_H_DEVICES,   ATLAS_STR_COUNT },
    { R_DEVICES,     K_ACTION, ATLAS_STR_SET_DEVICES,     ATLAS_STR_SET_D_DEVICES },
    { R_USB_RETRY,   K_ACTION, ATLAS_STR_SET_USB_RETRY,   ATLAS_STR_SET_D_USB_RETRY },

    { R_H_APPS,      K_HEAD,   ATLAS_STR_SET_H_APPS,      ATLAS_STR_COUNT },
    { R_APPS,        K_ACTION, ATLAS_STR_SET_APPS,        ATLAS_STR_SET_D_APPS },
    { R_CLEAR,       K_ACTION, ATLAS_STR_SET_CLEAR,       ATLAS_STR_SET_D_CLEAR },

    { R_H_BOOT,      K_HEAD,   ATLAS_STR_SET_H_BOOT,      ATLAS_STR_COUNT },
    { R_DEFAULT_APP, K_VALUE,  ATLAS_STR_SET_DEFAULT_APP, ATLAS_STR_SET_D_DEFAULT_APP },
    { R_TIMEOUT,     K_VALUE,  ATLAS_STR_SET_TIMEOUT,     ATLAS_STR_SET_D_TIMEOUT },

    { R_H_LANG,      K_HEAD,   ATLAS_STR_SET_H_LANG,      ATLAS_STR_COUNT },
    { R_LANG,        K_VALUE,  ATLAS_STR_SET_LANG,        ATLAS_STR_SET_D_LANG },

    { R_H_SYSTEM,    K_HEAD,   ATLAS_STR_SET_H_SYSTEM,    ATLAS_STR_COUNT },
    { R_SYSINFO,     K_ACTION, ATLAS_STR_SET_SYSINFO,     ATLAS_STR_SET_D_SYSINFO },
    { R_CFG_FILE,    K_INFO,   ATLAS_STR_SET_CFG_FILE,    ATLAS_STR_COUNT },

    { R_H_ADVANCED,  K_HEAD,   ATLAS_STR_SET_H_ADVANCED,  ATLAS_STR_COUNT },
    { R_RESET,       K_ACTION, ATLAS_STR_SET_RESET,       ATLAS_STR_SET_D_RESET },

    { R_H_RECOVERY,  K_HEAD,   ATLAS_STR_SET_H_RECOVERY,  ATLAS_STR_COUNT },
    { R_RECOVERY,    K_ACTION, ATLAS_STR_SET_RECOVERY,    ATLAS_STR_SET_D_RECOVERY },

    { R_H_ABOUT,     K_HEAD,   ATLAS_STR_SET_H_ABOUT,     ATLAS_STR_COUNT },
    { R_ABOUT_VER,   K_INFO,   ATLAS_STR_SYS_VERSION,     ATLAS_STR_COUNT },
    { R_ABOUT_LICENSE, K_INFO, ATLAS_STR_SET_ABOUT_LICENSE, ATLAS_STR_SET_ABOUT_NOTE },

    { R_SAVE,        K_ACTION, ATLAS_STR_SET_SAVE,        ATLAS_STR_SET_D_SAVE }
};

/* ------------------------------------------------------------------ */
/* State                                                               */
/*                                                                     */
/* Only the six fields this screen edits, not a whole atlas_config_t.   */
/* Holding the whole struct would mean Save wrote back a video block    */
/* captured before the user opened the video screen from row 5.         */
/* ------------------------------------------------------------------ */

typedef struct {
    int cursor;
    int top;

    atlas_lang_t    lang;
    atlas_startup_t startup;
    int             animations;
    int             sounds;
    char            default_app[ATLAS_CFG_PATH_MAX];
    int             timeout;

    /** What the language was on entry, for the unsaved-exit revert. */
    atlas_lang_t lang_on_entry;

    /** Set once the fields above have been read from ATLAS.INI. */
    int loaded;

    /** Non-zero when the fields differ from what is on the card. */
    int dirty;

    /** Non-zero once Back has warned about unsaved changes. */
    int warned;

    /** Which action a confirmation box is currently asking about. */
    int        confirming;
    set_row_t  confirm_row;

    atlas_str_id_t status;
    int            status_bad;
} set_state_t;

static set_state_t s_state;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void say(set_state_t *st, atlas_str_id_t message, int bad)
{
    st->status = message;
    st->status_bad = bad;
}

/** Mark the fields as differing from the file, and clear a stale note. */
static void touched(set_state_t *st)
{
    st->dirty = 1;
    st->warned = 0;
    say(st, ATLAS_STR_COUNT, 0);
}

static int selectable(const set_entry_t *e)
{
    return e->kind == K_VALUE || e->kind == K_ACTION;
}

/** First selectable row at or after `from`, wrapping. Never -1. */
static int next_row(int from, int step)
{
    int i, r = from;

    for (i = 0; i < R_COUNT; i++) {
        r = (r + R_COUNT) % R_COUNT;
        if (selectable(&s_entries[r]))
            return r;
        r += step;
    }

    /* Unreachable: the table has selectable rows. Returning 0 rather
     * than -1 keeps every caller free of a null check. */
    return 0;
}

/**
 * Which catalogue entry has this path, or -1.
 *
 * The same resolution the applications screen does, and it fails the
 * same way on purpose: a default application on an unplugged USB stick
 * is shown as its raw path rather than dropped, because dropping it
 * would silently clear a setting the user made.
 */
static int app_by_path(const char *path)
{
    int i, n = atlas_app_count();

    if (!path || !path[0])
        return -1;

    for (i = 0; i < n; i++) {
        const atlas_app_t *a = atlas_app_get(i);

        if (a && strcmp(a->path, path) == 0)
            return i;
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/* Loading and saving                                                  */
/* ------------------------------------------------------------------ */

static void load_fields(set_state_t *st)
{
    atlas_config_t cfg;
    atlas_config_origin_t origin;

    atlas_config_load(&cfg, &origin);

    st->lang       = cfg.lang;
    st->startup    = cfg.startup;
    st->animations = cfg.animations;
    st->sounds     = cfg.sounds;
    st->timeout    = cfg.timeout;

    snprintf(st->default_app, sizeof(st->default_app), "%s", cfg.default_app);

    /*
     * The running language wins over the file's. A boot that could not
     * read ATLAS.INI is showing English; making this screen claim the
     * file's French would put a label on a row that does not describe
     * what the user is looking at.
     */
    st->lang = atlas_i18n_lang();

    st->dirty = 0;
    st->warned = 0;
}

/**
 * Write this screen's six keys back to ATLAS.INI.
 *
 * Read-modify-write, exactly as the video screen does it: the block
 * this screen does not own is whatever the file already had, so a save
 * from here never undoes a mode the user set two screens deeper.
 */
static void save_fields(set_state_t *st)
{
    atlas_config_t cfg;
    atlas_config_origin_t origin;

    atlas_config_load(&cfg, &origin);

    cfg.lang       = st->lang;
    cfg.startup    = st->startup;
    cfg.animations = st->animations;
    cfg.sounds     = st->sounds;
    cfg.timeout    = st->timeout;

    snprintf(cfg.default_app, sizeof(cfg.default_app), "%s", st->default_app);

    if (atlas_config_save(&cfg) == ATLAS_OK) {
        st->dirty = 0;
        st->warned = 0;
        st->lang_on_entry = st->lang;
        say(st, ATLAS_STR_SET_SAVED, 0);
    } else {
        say(st, ATLAS_STR_VID_E_SAVE, 1);
    }
}

/**
 * Write the defaults over ATLAS.INI.
 *
 * Whole-file, unlike Save: this row says "reset all settings", so
 * keeping the video block would make it a lie. atlas_config_save()
 * rotates the previous file to ATLAS.INI.BAK, which is what the
 * description promises and what makes this undoable with a card
 * reader.
 *
 * The video mode is deliberately NOT re-applied here. Changing what is
 * on the television as a side effect of a menu press is the one thing
 * the video screen's countdown exists to prevent; the defaults take
 * effect on the next boot, where a bad result is escapable by holding
 * R1.
 */
static void reset_all(set_state_t *st)
{
    atlas_config_t cfg;

    atlas_config_defaults(&cfg);

    if (atlas_config_save(&cfg) != ATLAS_OK) {
        say(st, ATLAS_STR_VID_E_SAVE, 1);
        return;
    }

    ATLAS_LOG("SET", "settings reset to defaults");

    st->lang       = cfg.lang;
    st->startup    = cfg.startup;
    st->animations = cfg.animations;
    st->sounds     = cfg.sounds;
    st->timeout    = cfg.timeout;
    st->default_app[0] = '\0';

    /* The language is part of "all settings", so it changes here too -
     * visibly, which is the only way a user can tell it worked. */
    atlas_i18n_set_lang(st->lang);
    atlas_i18n_load_overrides();
    st->lang_on_entry = st->lang;

    st->dirty = 0;
    st->warned = 0;
    say(st, ATLAS_STR_SET_RESET_DONE, 0);
}

/* ------------------------------------------------------------------ */
/* Editing                                                             */
/* ------------------------------------------------------------------ */

/**
 * Step the default application through the catalogue.
 *
 * The scan is done here rather than on entry: it walks directories on
 * a Memory Card and costs a visible fraction of a second, and a user
 * who came to change the language should not pay for it. Tied to the
 * press that needs it, the pause is one the user asked for.
 */
static void cycle_default_app(set_state_t *st, int dir)
{
    int n, cur;

    if (!atlas_app_scanned())
        atlas_app_scan();

    n = atlas_app_count();
    if (n <= 0) {
        /* Nothing to choose from. Clearing it is still meaningful:
         * it is how a user removes a default whose device is gone. */
        st->default_app[0] = '\0';
        return;
    }

    /* -1 is "none", and it is a real position in the cycle rather than
     * a state you can only reach by deleting text: the list runs
     * none, app 0 ... app n-1, none. */
    cur = app_by_path(st->default_app);
    if (cur < 0 && st->default_app[0]) {
        /*
         * A path that does not resolve - an unplugged stick, usually.
         * One press moves off it and it is gone, which is honest: the
         * screen cannot offer a list position for a file it cannot
         * find.
         */
        st->default_app[0] = '\0';
        return;
    }

    cur += dir;

    if (cur < -1)
        cur = n - 1;
    if (cur >= n)
        cur = -1;

    if (cur < 0) {
        st->default_app[0] = '\0';
    } else {
        const atlas_app_t *a = atlas_app_get(cur);

        if (a)
            snprintf(st->default_app, sizeof(st->default_app), "%s", a->path);
    }
}

/** Left/Right on the selected row. */
static void adjust(set_state_t *st, int dir)
{
    switch (s_entries[st->cursor].row) {
    case R_STARTUP:
        st->startup = (atlas_startup_t)
            ((st->startup + ATLAS_STARTUP_COUNT + dir) % ATLAS_STARTUP_COUNT);
        break;

    case R_ANIM:
        st->animations = !st->animations;
        break;

    case R_SOUNDS:
        st->sounds = !st->sounds;
        break;

    case R_DEFAULT_APP:
        cycle_default_app(st, dir);
        break;

    case R_TIMEOUT:
        /*
         * Clamped, not wrapped. Wrapping would turn one press past the
         * end into an auto-launch the user did not ask for, or one
         * press past zero into a thirty-second wait.
         */
        st->timeout = ATLAS_CLAMP(st->timeout + dir, 0, ATLAS_CFG_TIMEOUT_MAX);
        break;

    case R_LANG:
        st->lang = (atlas_lang_t)
            ((st->lang + ATLAS_LANG_COUNT + dir) % ATLAS_LANG_COUNT);

        /*
         * Applied at once. set_lang() drops the loaded overrides
         * because they belonged to the old language, so the file for
         * the new one has to be read again - otherwise switching to
         * French and back would silently lose a translation file the
         * user installed.
         */
        atlas_i18n_set_lang(st->lang);
        atlas_i18n_load_overrides();
        break;

    default:
        return;
    }

    touched(st);
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

/** Actions that discard unsaved edits or leave this screen's stack. */
static int needs_confirmation(set_row_t row)
{
    return row == R_RESET || row == R_RECOVERY;
}

static void perform(set_state_t *st, set_row_t row)
{
    switch (row) {
    case R_VIDEO:
        atlas_screen_push(atlas_screen_video());
        break;

    case R_THEME:
        atlas_screen_push(atlas_screen_theme());
        break;

    case R_DEVICES:
        atlas_screen_push(atlas_screen_devices());
        break;

    case R_USB_RETRY:
        /*
         * The one repair this screen performs itself. Loading the USB
         * modules a second time is what turns "I plugged the stick in
         * after switching on" from a reboot into a button press, and
         * the boot module already guards against loading them twice.
         */
        if (atlas_boot_load_usb() == ATLAS_OK)
            say(st, ATLAS_STR_SET_USB_OK, 0);
        else
            say(st, ATLAS_STR_SET_USB_FAIL, 1);
        break;

    case R_APPS:
        atlas_screen_push(atlas_screen_apps());
        break;

    case R_CLEAR:
        /*
         * Both lists, and no application file. clear() rather than
         * reset() because the emptying has to reach the card: reset()
         * leaves the lists clean, and a save after it would do
         * nothing, so the favorites would come back on the next boot.
         */
        atlas_fav_clear();

        if (atlas_fav_save() == ATLAS_OK)
            say(st, ATLAS_STR_SET_CLEARED, 0);
        else
            say(st, ATLAS_STR_VID_E_SAVE, 1);
        break;

    case R_SYSINFO:
        atlas_screen_push(atlas_screen_sysinfo());
        break;

    case R_RESET:
        reset_all(st);
        break;

    case R_RECOVERY:
        /*
         * A reset, not a push. Recovery is a root: it forces the
         * built-in theme on and its own "continue" resets back to
         * Home, so leaving it on top of this screen would leave a
         * Back that fell through into a menu it had just replaced the
         * theme of.
         *
         * The real recovery mode is still the one held at boot, which
         * is what the row's description says: that one has read no
         * settings at all, and this one has.
         */
        ATLAS_LOG("SET", "entering recovery tools from settings");
        atlas_screen_reset(atlas_screen_recovery());
        break;

    case R_SAVE:
        save_fields(st);
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static void set_enter(atlas_screen_t *self)
{
    set_state_t *st = (set_state_t *)self->data;

    /*
     * Read once. enter() also runs when a pushed screen pops back to
     * here, and re-reading then would throw away edits the user made
     * before opening it.
     */
    if (!st->loaded) {
        load_fields(st);
        st->lang_on_entry = st->lang;
        st->cursor = next_row(0, 1);
        st->top = 0;
        st->loaded = 1;
    }

    st->confirming = 0;
    st->status = ATLAS_STR_COUNT;
    st->status_bad = 0;
}

static void set_update(atlas_screen_t *self)
{
    set_state_t *st = (set_state_t *)self->data;
    u32 rep = atlas_input_repeated();

    /* The confirmation owns input while it is up. */
    if (st->confirming) {
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
            st->confirming = 0;
            perform(st, st->confirm_row);
        } else if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
            st->confirming = 0;
        }
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        /*
         * One warning, then out. A settings screen that refused to
         * close would be worse than one that loses an edit: the user
         * can always come back, and they cannot always work out which
         * button the dialog wants.
         */
        if (st->dirty && !st->warned) {
            st->warned = 1;
            say(st, ATLAS_STR_SET_UNSAVED, 1);
            return;
        }

        /*
         * Make the warning true. The language was applied live, so
         * leaving without saving has to put the old one back -
         * otherwise "leaving now puts back the previous settings"
         * would be false for the one setting the user can see.
         */
        if (st->dirty && st->lang != st->lang_on_entry) {
            atlas_i18n_set_lang(st->lang_on_entry);
            atlas_i18n_load_overrides();
        }

        st->loaded = 0;
        atlas_screen_pop();
        return;
    }

    if (rep & ATLAS_BTN_UP)
        st->cursor = next_row(st->cursor - 1, -1);

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = next_row(st->cursor + 1, 1);

    /*
     * Keep the cursor in the window, and pull the heading above it into
     * view when there is one: a row scrolled to the top with its
     * section title just off screen looks like it belongs to the
     * section before.
     */
    if (st->cursor < st->top)
        st->top = st->cursor;
    if (st->cursor >= st->top + SET_VISIBLE)
        st->top = st->cursor - SET_VISIBLE + 1;
    if (st->top > 0 && st->top == st->cursor
        && s_entries[st->top - 1].kind == K_HEAD)
        st->top--;

    if (rep & ATLAS_BTN_LEFT)
        adjust(st, -1);
    else if (rep & ATLAS_BTN_RIGHT)
        adjust(st, +1);

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        const set_entry_t *e = &s_entries[st->cursor];

        if (e->kind != K_ACTION)
            return;

        if (needs_confirmation(e->row)) {
            st->confirming = 1;
            st->confirm_row = e->row;
        } else {
            perform(st, e->row);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/** The right-hand value for one row, or NULL when it carries none. */
static const char *row_value(const set_state_t *st, set_row_t row, char *buf,
                             int size)
{
    switch (row) {
    case R_STARTUP:
        return atlas_str(st->startup == ATLAS_STARTUP_APPS
                         ? ATLAS_STR_HOME_APPS
                         : ATLAS_STR_SET_STARTUP_HOME);

    case R_ANIM:
        return atlas_str(st->animations ? ATLAS_STR_SET_ON
                                        : ATLAS_STR_SET_OFF);

    case R_SOUNDS:
        return atlas_str(st->sounds ? ATLAS_STR_SET_ON : ATLAS_STR_SET_OFF);

    case R_DEFAULT_APP: {
        int i;

        if (!st->default_app[0])
            return atlas_str(ATLAS_STR_SET_NONE);

        i = app_by_path(st->default_app);
        if (i >= 0) {
            const atlas_app_t *a = atlas_app_get(i);

            if (a)
                return a->name;
        }

        /* Unresolved: the raw path, because it is the only thing that
         * tells the user which file the setting still names. */
        return st->default_app;
    }

    case R_TIMEOUT:
        if (st->timeout <= 0)
            return atlas_str(ATLAS_STR_SET_TIMEOUT_OFF);

        snprintf(buf, size, "%d s", st->timeout);
        return buf;

    case R_LANG:
        return atlas_str(st->lang == ATLAS_LANG_FR ? ATLAS_STR_LANG_FR
                                                   : ATLAS_STR_LANG_EN);

    case R_CFG_FILE: {
        const char *src = atlas_config_source();

        return (src && src[0]) ? src : atlas_str(ATLAS_STR_SET_CFG_NONE);
    }

    case R_ABOUT_VER:
        return ATLAS_VERSION_STRING;

    case R_ABOUT_LICENSE:
        /* Not translated: the licence is named by its identifier, and
         * an identifier that changes with the language names nothing. */
        return "MIT";

    default:
        return NULL;
    }
}

static void draw_row(const set_state_t *st, int i, float x, float y, float w)
{
    const set_entry_t *e = &s_entries[i];
    const atlas_theme_t *t = atlas_theme();
    float lh = atlas_ui_line_height();
    float row_y = y + ((float)ATLAS_UI_ROW_H - lh) * 0.5f;
    char buf[64];

    if (e->kind == K_HEAD) {
        /* No panel behind it: a heading with the same slab as a row
         * looks like one more thing to press Cross on. */
        atlas_ui_text(x + (float)ATLAS_UI_PAD * 0.5f, row_y,
                      ATLAS_ALIGN_LEFT, t->accent, atlas_str(e->label));
        return;
    }

    if (e->kind == K_INFO) {
        const char *v = row_value(st, e->row, buf, sizeof(buf));

        atlas_ui_text(x + (float)ATLAS_UI_PAD, row_y, ATLAS_ALIGN_LEFT,
                      t->text_dim, atlas_str(e->label));

        if (v)
            atlas_ui_text_clipped(x + w * 0.42f, row_y, t->text_dim, v,
                                  w * 0.58f - (float)ATLAS_UI_PAD);
        return;
    }

    atlas_ui_menu_row(x, y, w, i == st->cursor, atlas_str(e->label),
                      row_value(st, e->row, buf, sizeof(buf)));
}

static void set_draw(atlas_screen_t *self)
{
    set_state_t *st = (set_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i, last;
    char hints[160];

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_SETTINGS));

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_SETTINGS));
    y += lh * 1.9f;

    last = st->top + SET_VISIBLE;
    if (last > R_COUNT)
        last = R_COUNT;

    for (i = st->top; i < last; i++) {
        draw_row(st, i, x, y, w * 0.78f);
        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    y += lh * 0.4f;

    if (s_entries[st->cursor].detail != ATLAS_STR_COUNT)
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(s_entries[st->cursor].detail), w);

    if (st->status != ATLAS_STR_COUNT) {
        y += lh * 1.4f;
        atlas_ui_text_clipped(x, y, st->status_bad ? t->warn : t->ok,
                              atlas_str(st->status), w);
    }

    snprintf(hints, sizeof(hints), "<>  %s     X  %s     O  %s",
             atlas_str(ATLAS_STR_CHANGE),
             atlas_str(ATLAS_STR_SELECT),
             atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);

    if (st->confirming) {
        const set_entry_t *e = &s_entries[st->cursor];

        snprintf(hints, sizeof(hints), "X  %s     O  %s",
                 atlas_str(ATLAS_STR_CONFIRM), atlas_str(ATLAS_STR_CANCEL));

        atlas_ui_message_box(atlas_str(e->label),
                             e->detail != ATLAS_STR_COUNT
                                 ? atlas_str(e->detail) : "",
                             hints);
    }
}

static atlas_screen_t s_screen = {
    "Settings",
    set_enter,
    NULL,
    set_update,
    set_draw,
    &s_state
};

atlas_screen_t *atlas_screen_settings(void)
{
    return &s_screen;
}
