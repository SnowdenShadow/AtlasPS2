/*
 * AtlasPS2 - screen_wizard.c
 * The first-boot wizard.
 *
 * Three questions on one screen, and then Home. The spec is explicit
 * that this must not become fifteen configuration screens, and there is
 * a reason beyond taste: everything asked here has a working default
 * and a row in Settings. A wizard that insisted on choices would be
 * asking a user who has seen the program for four seconds to make
 * decisions they have no basis for.
 *
 * WHY DISPLAY IS SHOWN BUT NOT EDITABLE
 * ------------------------------------
 * The spec's own sketch offers "Display: [ Automatic ]" with no
 * alternatives, and that is the honest shape. A television that cannot
 * show the current mode is exactly the case where the user cannot read
 * this screen to fix it - the escape for that is holding R1 at boot,
 * which the row says. Offering PAL/NTSC here would be offering the one
 * setting capable of making the wizard itself invisible, without the
 * confirmation countdown that the video screen has and this does not.
 *
 * WHY A FAILED SAVE IS NOT AN ERROR SCREEN
 * ---------------------------------------
 * A Memory Card that will not take a write is a real problem, but it is
 * not one that should stop the console reaching its menu. The wizard
 * says so and continues; the only cost is being asked again next boot,
 * which is also the correct behaviour, since nothing was recorded.
 */
#include <stdio.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/config.h"
#include "atlas/device.h"
#include "atlas/app.h"
#include "atlas/i18n.h"
#include "atlas/log.h"

typedef enum {
    W_LANG = 0,   /**< Left/Right pick the language, applied live */
    W_DISPLAY,    /**< shown, not editable - see the header       */
    W_SCAN,       /**< Left/Right toggle yes/no                   */
    W_START,      /**< Cross saves and leaves                     */
    W_COUNT
} wiz_row_t;

typedef struct {
    int cursor;
    atlas_lang_t lang;
    int scan;       /**< 1 = scan on the way out */
    int scanning;   /**< a scan was requested and has not run yet */
    int found;      /**< result of that scan, -1 before it ran    */
    int failed;     /**< the settings write did not succeed       */
} wiz_state_t;

static wiz_state_t s_state;

static void wiz_enter(atlas_screen_t *self)
{
    wiz_state_t *st = (wiz_state_t *)self->data;

    st->cursor = W_LANG;
    st->scan = 1;
    st->scanning = 0;
    st->found = -1;
    st->failed = 0;

    /*
     * Starts from whatever is live rather than from a fixed language:
     * on a console whose region the loader could read, the sensible
     * default is already in place, and resetting to English here would
     * throw that away.
     */
    st->lang = atlas_i18n_lang();
}

/**
 * Write the three answers, leaving every other setting at its default.
 *
 * A whole-file write is right here and nowhere else: this runs only
 * when no configuration file was found, so there is nothing to preserve
 * and no other section that could be clobbered. Everywhere else in
 * AtlasPS2 saving is read-modify-write for exactly the opposite reason.
 */
static void save_answers(wiz_state_t *st)
{
    atlas_config_t cfg;

    atlas_config_defaults(&cfg);
    cfg.lang = st->lang;

    if (atlas_config_save(&cfg) != ATLAS_OK) {
        ATLAS_LOG("WIZ", "could not write the first configuration");
        st->failed = 1;
    }
}

static void wiz_update(atlas_screen_t *self)
{
    wiz_state_t *st = (wiz_state_t *)self->data;
    u32 rep = atlas_input_repeated();
    int dir;

    /*
     * The scan runs here, one frame after the press that asked for it,
     * so that the "Scanning..." line is actually on screen while the
     * card is being walked. Doing it inside the button handler would
     * draw that line for zero frames: the walk would finish before the
     * next frame was ever presented.
     */
    if (st->scanning) {
        st->found = atlas_app_scan();
        st->scanning = 0;
        return;
    }

    if (st->failed) {
        /* One acknowledgement, then Home either way. */
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)
         || atlas_input_is_pressed(ATLAS_BTN_BACK))
            atlas_screen_reset(atlas_screen_home());
        return;
    }

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + W_COUNT - 1) % W_COUNT;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % W_COUNT;

    dir = (rep & ATLAS_BTN_RIGHT) ? 1 : (rep & ATLAS_BTN_LEFT) ? -1 : 0;

    if (dir) {
        switch (st->cursor) {
        case W_LANG:
            st->lang = (atlas_lang_t)
                       ((st->lang + ATLAS_LANG_COUNT + dir) % ATLAS_LANG_COUNT);
            /*
             * Applied immediately, because the whole point of this row
             * is that the user can see which language they picked. The
             * overrides are re-read because setting the language drops
             * them.
             */
            atlas_i18n_set_lang(st->lang);
            atlas_i18n_load_overrides();
            break;

        case W_SCAN:
            st->scan = !st->scan;
            break;

        default:
            break;
        }
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        if (st->cursor == W_START) {
            save_answers(st);

            if (st->scan && st->found < 0) {
                st->scanning = 1;   /* drawn first, run next frame */
                return;
            }

            if (!st->failed)
                atlas_screen_reset(atlas_screen_home());
        } else {
            /* Cross on a question row moves on, so the wizard can be
             * finished with four presses of one button. */
            st->cursor++;
        }
    }

    /*
     * No Back. There is nowhere behind this screen - it is the root on
     * a first boot - and a wizard that could be dismissed into a menu
     * would leave the console asking the same questions every time it
     * started, which is worse than asking once.
     */
}

/** The value column for one row, as text. */
static const char *row_value(const wiz_state_t *st, wiz_row_t row)
{
    switch (row) {
    case W_LANG:
        return atlas_str(st->lang == ATLAS_LANG_FR
                         ? ATLAS_STR_LANG_FR : ATLAS_STR_LANG_EN);
    case W_DISPLAY:
        return atlas_str(ATLAS_STR_WIZ_AUTO);
    case W_SCAN:
        return atlas_str(st->scan ? ATLAS_STR_WIZ_YES : ATLAS_STR_WIZ_NO);
    default:
        return NULL;
    }
}

static atlas_str_id_t row_label(wiz_row_t row)
{
    switch (row) {
    case W_LANG:    return ATLAS_STR_WIZ_LANG;
    case W_DISPLAY: return ATLAS_STR_WIZ_DISPLAY;
    case W_SCAN:    return ATLAS_STR_WIZ_SCAN;
    default:        return ATLAS_STR_WIZ_START;
    }
}

static atlas_str_id_t row_detail(wiz_row_t row)
{
    switch (row) {
    case W_DISPLAY: return ATLAS_STR_WIZ_D_DISPLAY;
    case W_SCAN:    return ATLAS_STR_WIZ_D_SCAN;
    default:        return ATLAS_STR_COUNT;
    }
}

static void wiz_draw(atlas_screen_t *self)
{
    wiz_state_t *st = (wiz_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float row_w = sw * 0.66f;
    float y;
    int i;
    atlas_str_id_t detail;
    char buf[96];

    atlas_ui_header(NULL);

    y = atlas_ui_content_y();
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_WIZ_TITLE));
    y = atlas_ui_content_y_titled();

    atlas_ui_text_clipped(x, y, t->text_dim, atlas_str(ATLAS_STR_WIZ_INTRO),
                          sw - (float)ATLAS_UI_PAD * 2.0f);
    y += atlas_ui_line_height() * 2.0f;

    for (i = 0; i < W_COUNT; i++) {
        atlas_ui_menu_row(x, y, row_w, i == st->cursor,
                          atlas_str(row_label((wiz_row_t)i)),
                          row_value(st, (wiz_row_t)i));
        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    y += atlas_ui_line_height() * 0.6f;

    /*
     * One explanatory line, for the row the cursor is on. The scan
     * result replaces it once there is one, because at that point the
     * question has been answered and the number is what the user wants
     * to see.
     */
    if (st->scanning) {
        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                      atlas_str(ATLAS_STR_WIZ_SCANNING));
    } else if (st->found >= 0) {
        snprintf(buf, sizeof(buf), atlas_str(ATLAS_STR_WIZ_FOUND), st->found);
        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim, buf);
    } else {
        detail = row_detail((wiz_row_t)st->cursor);
        if (detail != ATLAS_STR_COUNT)
            atlas_ui_text_clipped(x, y, t->text_dim, atlas_str(detail),
                                  sw - (float)ATLAS_UI_PAD * 2.0f);
    }

    snprintf(buf, sizeof(buf), "<>  %s     X  %s",
             atlas_str(ATLAS_STR_CHANGE), atlas_str(ATLAS_STR_SELECT));
    atlas_ui_footer(buf);

    if (st->failed) {
        snprintf(buf, sizeof(buf), "X  %s", atlas_str(ATLAS_STR_OK));
        atlas_ui_message_box(atlas_str(ATLAS_STR_WIZ_TITLE),
                             atlas_str(ATLAS_STR_WIZ_SAVE_FAIL), buf);
    }
}

static atlas_screen_t s_screen = {
    "Wizard",
    wiz_enter,
    NULL,
    wiz_update,
    wiz_draw,
    &s_state
};

atlas_screen_t *atlas_screen_wizard(void)
{
    return &s_screen;
}
