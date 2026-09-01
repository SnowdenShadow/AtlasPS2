/*
 * AtlasPS2 - screen_theme.c
 * Choosing a theme from the ones present on the attached devices.
 *
 * The list is what is actually there, plus a first row for the built-in
 * theme that is always there. A text field the user types a folder name
 * into would be a field where a typo produces "no such theme" and no
 * way to find out what the right spelling was - on a console with no
 * keyboard, at that.
 *
 * A theme is applied the moment it is highlighted rather than on
 * confirm. Colours are the one setting whose whole meaning is how it
 * looks, and a preview that costs a press and a screen transition is a
 * preview nobody uses. That is safe here in a way it is not on the
 * video screen: a palette cannot stop a television from displaying a
 * picture, so the worst an unreadable theme can do is send the user
 * back up this list, which the built-in row at the top always ends.
 *
 * Nothing is written to ATLAS.INI until Save is chosen.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"

#include "atlas/config.h"
#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/log.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

/*
 * More than a Memory Card is likely to hold, and the scan stops here
 * rather than growing: a menu longer than this is one nobody scrolls
 * through anyway.
 */
#define THEME_MAX     16

/*
 * Rows are counted at draw time rather than fixed: see the note in
 * screen_apps.c. The reserve is the description line under the list,
 * the position counter, and the "no themes found" note.
 */
static int theme_visible(void)
{
    return atlas_ui_rows_fit(atlas_ui_content_y_titled(),
                             atlas_ui_line_height() * 3.4f);
}

/* Row 0 is the built-in theme, so a theme at list index i is row i + 1
 * and the Save row is last. */
#define ROW_BUILTIN 0

typedef struct {
    int  cursor;
    int  top;

    char names[THEME_MAX][ATLAS_THEME_NAME_MAX];
    int  count;

    /** Which row was last applied, so a preview runs once per move. */
    int  applied;

    /** The theme that was live on entry, restored on an unsaved Back. */
    char entry[ATLAS_THEME_NAME_MAX];

    atlas_str_id_t status;
    int            status_bad;
} theme_state_t;

static theme_state_t s_state;

/** Rows: the built-in one, each theme found, then Save. */
static int row_count(const theme_state_t *st)
{
    return st->count + 2;
}

static int row_is_save(const theme_state_t *st, int row)
{
    return row == st->count + 1;
}

static void say(theme_state_t *st, atlas_str_id_t message, int bad)
{
    st->status = message;
    st->status_bad = bad;
}

/* ------------------------------------------------------------------ */
/* Applying                                                            */
/* ------------------------------------------------------------------ */

/**
 * Make row `row` the live palette.
 *
 * A theme that cannot be read leaves the built-in one active and says
 * so, rather than leaving the previous theme on screen: the user needs
 * to see that the row they are sitting on is not the thing they are
 * looking at.
 */
static void apply_row(theme_state_t *st, int row)
{
    if (row == st->applied)
        return;

    st->applied = row;

    if (row == ROW_BUILTIN) {
        atlas_theme_set(NULL);
        say(st, ATLAS_STR_COUNT, 0);
        return;
    }

    if (atlas_theme_load(st->names[row - 1]) == ATLAS_OK) {
        say(st, ATLAS_STR_COUNT, 0);
        return;
    }

    atlas_theme_set(NULL);
    say(st, ATLAS_STR_THEME_E_LOAD, 1);
}

/**
 * Write the chosen theme to ATLAS.INI.
 *
 * Reads the file first and replaces only the theme name, so a save from
 * here does not overwrite video settings or a default application the
 * user set on another screen.
 */
static void save_theme(theme_state_t *st)
{
    atlas_config_t cfg;
    atlas_config_origin_t origin;
    const char *live = atlas_theme_name();

    atlas_config_load(&cfg, &origin);

    /*
     * What is on the screen, not what the cursor last passed over. A
     * theme that failed to load left the built-in palette up, and
     * writing its name anyway would save a setting that produces
     * exactly the failure the user just saw, every boot from now on.
     *
     * "default" rather than an empty string: the field is what the user
     * reads in the file, and a blank value looks like a setting that
     * failed to write rather than a deliberate choice.
     */
    if (live[0] == '\0')
        snprintf(cfg.theme, sizeof(cfg.theme), "default");
    else
        snprintf(cfg.theme, sizeof(cfg.theme), "%s", live);

    if (atlas_config_save(&cfg) == ATLAS_OK) {
        /* Now the theme to go back to, so Back stops undoing it. */
        snprintf(st->entry, sizeof(st->entry), "%s", live);
        say(st, ATLAS_STR_THEME_SAVED, 0);
    } else {
        say(st, ATLAS_STR_THEME_E_SAVE, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Screen                                                              */
/* ------------------------------------------------------------------ */

static void theme_enter(atlas_screen_t *self)
{
    theme_state_t *st = (theme_state_t *)self->data;
    const char *live = atlas_theme_name();
    int i;

    st->cursor = 0;
    st->top = 0;
    st->status = ATLAS_STR_COUNT;
    st->status_bad = 0;

    snprintf(st->entry, sizeof(st->entry), "%s", live);

    /*
     * Scanned on entry rather than cached: a user who plugs in a stick,
     * comes here and finds nothing has no reason to suspect a stale
     * list, and the scan is a handful of directory opens.
     */
    st->count = atlas_theme_list(st->names, THEME_MAX);

    ATLAS_LOG("THEME", "%d theme(s) available", st->count);

    /*
     * Start on the theme already in use, so leaving without touching
     * anything is what it looks like. The name comes from the theme
     * module, not from ATLAS.INI, because a theme named in the file
     * that failed to load is not the one on screen.
     */
    st->applied = ROW_BUILTIN;

    for (i = 0; i < st->count; i++) {
        if (strcmp(st->names[i], live) == 0) {
            st->applied = i + 1;
            st->cursor = i + 1;
            break;
        }
    }

    if (st->cursor >= theme_visible())
        st->top = st->cursor - theme_visible() + 1;
}

/**
 * Leaving undoes an unsaved preview.
 *
 * Without this a user could walk through five themes, press Back, and
 * be left in the fifth - a change they never asked to keep, which the
 * next boot would then undo on its own. That is the shape of a setting
 * nobody can reason about.
 *
 * Restored from what was live on entry rather than by re-reading
 * ATLAS.INI. The two differ after a boot where the configured theme
 * failed to load, and re-reading would try that failing theme again on
 * the way out of the screen the user came here to fix it from.
 */
static void theme_leave(atlas_screen_t *self)
{
    theme_state_t *st = (theme_state_t *)self->data;

    if (strcmp(st->entry, atlas_theme_name()) == 0)
        return;

    if (st->entry[0] == '\0' || atlas_theme_load(st->entry) != ATLAS_OK)
        atlas_theme_set(NULL);
}

static void theme_update(atlas_screen_t *self)
{
    theme_state_t *st = (theme_state_t *)self->data;
    int rep = atlas_input_repeated();
    int count = row_count(st);

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + count - 1) % count;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % count;

    if (st->cursor < st->top)
        st->top = st->cursor;
    if (st->cursor >= st->top + theme_visible())
        st->top = st->cursor - theme_visible() + 1;

    /* The preview. Save is not a theme, so sitting on it leaves the
     * last previewed palette alone rather than reverting it. */
    if (!row_is_save(st, st->cursor))
        apply_row(st, st->cursor);

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM) && row_is_save(st, st->cursor))
        save_theme(st);

    if (atlas_input_is_pressed(ATLAS_BTN_BACK))
        atlas_screen_pop();
}

static void theme_draw(atlas_screen_t *self)
{
    theme_state_t *st = (theme_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int count = row_count(st);
    int last, i;
    char hints[128];

    atlas_ui_header(atlas_video_mode_name());

    y = atlas_ui_content_y();
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_THEME_TITLE));
    y = atlas_ui_content_y_titled();

    last = st->top + theme_visible();
    if (last > count)
        last = count;

    for (i = st->top; i < last; i++) {
        const char *label;
        const char *value = NULL;

        if (row_is_save(st, i))
            label = atlas_str(ATLAS_STR_THEME_SAVE);
        else if (i == ROW_BUILTIN)
            label = atlas_str(ATLAS_STR_THEME_BUILTIN);
        else
            label = st->names[i - 1];

        /*
         * The mark goes on the row matching the live palette, asked of
         * the theme module rather than of st->applied. They differ in
         * exactly the case that matters: a theme that failed to load
         * leaves the built-in colours on screen, and marking the row
         * the cursor is on would claim the user is looking at a theme
         * they are not.
         */
        if (!row_is_save(st, i)) {
            const char *live = atlas_theme_name();

            if (i == ROW_BUILTIN ? live[0] == '\0'
                                 : strcmp(st->names[i - 1], live) == 0)
                value = "*";
        }

        atlas_ui_menu_row(x, y, w * 0.70f, i == st->cursor, label, value);

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    if (count > theme_visible()) {
        char pos[32];

        snprintf(pos, sizeof(pos), "%d / %d", st->cursor + 1, count);
        atlas_ui_text(x + w * 0.70f, y + (float)ATLAS_UI_ROW_GAP,
                      ATLAS_ALIGN_RIGHT, t->text_dim, pos);
    }

    y += lh * 0.4f;

    if (row_is_save(st, st->cursor))
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_THEME_D_SAVE), w);
    else if (st->cursor == ROW_BUILTIN)
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_THEME_D_BUILTIN), w);
    else
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_THEME_D_ONE), w);

    y += lh * 1.3f;

    /*
     * Said where a user staring at an empty list is standing, not only
     * in the documentation: "no themes" and "themes go in the wrong
     * folder" look identical from here.
     */
    if (st->count == 0) {
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_THEME_NONE), w);
        y += lh * 1.2f;
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_THEME_NONE_HINT), w);
    } else {
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_THEME_NOTE), w);
    }

    if (st->status != ATLAS_STR_COUNT) {
        y += lh * 1.3f;
        atlas_ui_text_clipped(x, y, st->status_bad ? t->warn : t->ok,
                              atlas_str(st->status), w);
    }

    snprintf(hints, sizeof(hints), "X  %s     O  %s",
             atlas_str(ATLAS_STR_SELECT), atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);
}

static atlas_screen_t s_screen = {
    "Theme",
    theme_enter,
    theme_leave,
    theme_update,
    theme_draw,
    &s_state
};

atlas_screen_t *atlas_screen_theme(void)
{
    return &s_screen;
}
