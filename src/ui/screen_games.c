/*
 * AtlasPS2 - screen_games.c
 * The disc images a scan found, and starting one.
 *
 * WHY THIS SCREEN HAS A WARNING AND THE APPLICATIONS SCREEN DOES NOT
 * ------------------------------------------------------------------
 * Launching an ELF is a thing this project has done since Milestone 4
 * and can report a failure from: the loader either accepts the file or
 * it does not, and either way somebody is still looking at a menu.
 *
 * Starting a game is not that. Past atlas_discboot_run()'s IOP reset
 * there is no video, no pad, no logging and no way back except the
 * power switch - and the module that answers the game's disc reads has
 * never run on a console. A black screen is a possible outcome of
 * pressing Cross here, so the user is told that before they press it,
 * once, in a dialog they have to confirm.
 *
 * That dialog is not politeness. A user who was warned power-cycles and
 * reports a game; a user who was not assumes they have bricked
 * something.
 *
 * WHAT THE LIST SHOWS AND WHY IT IS SO LITTLE
 * -------------------------------------------
 * Filenames. Identifying an image means reading its volume descriptor
 * and SYSTEM.CNF, which costs enough per file that doing it for a
 * folder of thirty on entry would be a stall. So the identification
 * happens for exactly one image, when it is chosen, and its result is
 * shown in the confirmation dialog - which is also where a file that
 * turns out not to be a PS2 disc is reported.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/game.h"
#include "atlas/discboot.h"
#include "atlas/device.h"
#include "atlas/i18n.h"

/*
 * Rows are counted at draw time rather than fixed: see the note in
 * screen_apps.c. The reserve covers the selected image's path and the
 * position counter under the list.
 */
static int games_visible(void)
{
    return atlas_ui_rows_fit(atlas_ui_content_y(),
                             atlas_ui_line_height() * 2.6f);
}

typedef enum {
    GS_LIST = 0,   /* browsing                                   */
    GS_CONFIRM,    /* identified, warning shown, awaiting Cross  */
    GS_FAILED      /* prepare() refused; the reason is on screen */
} games_mode_t;

typedef struct {
    int cursor;
    int top;

    games_mode_t mode;

    /*
     * The identified image, filled by prepare() when a row is chosen.
     * Kept whole rather than as a few extracted fields because
     * atlas_discboot_run() takes exactly this and must be handed what
     * prepare() produced, not a reconstruction of it.
     */
    atlas_discboot_t ready;

    atlas_str_id_t fail_reason;
} games_state_t;

static games_state_t s_state;

/* ------------------------------------------------------------------ */
/* Entering and leaving                                                */
/* ------------------------------------------------------------------ */

static void games_enter(atlas_screen_t *self)
{
    games_state_t *st = (games_state_t *)self->data;

    st->mode = GS_LIST;
    st->fail_reason = ATLAS_STR_GAMES_FAIL_OTHER;

    if (!atlas_game_scanned())
        atlas_game_scan();

    if (st->cursor >= atlas_game_count())
        st->cursor = 0;
    if (st->cursor < 0)
        st->cursor = 0;

    st->top = 0;
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

/*
 * Kept as a key rather than as resolved text, for the same reason the
 * applications screen does it: the box can stay up across a language
 * change, and half-translated is the worst state for an error message.
 */
static atlas_str_id_t prepare_message(atlas_err_t err)
{
    switch (err) {
    case ATLAS_ENOENT:  return ATLAS_STR_GAMES_FAIL_GONE;
    case ATLAS_EFORMAT: return ATLAS_STR_GAMES_FAIL_FORMAT;
    case ATLAS_ENOMEM:  return ATLAS_STR_GAMES_FAIL_FRAG;
    case ATLAS_ENODEV:  return ATLAS_STR_GAMES_FAIL_DEV;
    default:            return ATLAS_STR_GAMES_FAIL_OTHER;
    }
}

/**
 * Identify the image under the cursor and decide what to show next.
 *
 * Everything that can fail while there is still a screen happens here.
 * That is the whole reason atlas_discboot_prepare() is a separate call
 * from atlas_discboot_run(): a truncated download, a ZSO with a bad
 * index and a file on the wrong device are each a message, and past
 * run()'s IOP reset none of them could be one.
 */
static void choose(games_state_t *st)
{
    const atlas_game_t *g = atlas_game_get(st->cursor);
    atlas_err_t err;

    if (!g)
        return;

    if (g->is_hdl == 2) {
        st->mode = GS_FAILED;
        st->fail_reason = ATLAS_STR_GAMES_FAIL_HDL;
        return;
    }

    err = g->is_hdl
        ? atlas_discboot_prepare_hdl(g->hdl_start_lba, g->hdl_total_sectors,
                                     g->name, &st->ready)
        : atlas_discboot_prepare(g->path, &st->ready);

    if (err != ATLAS_OK) {
        st->mode = GS_FAILED;
        st->fail_reason = prepare_message(err);
        return;
    }

    st->mode = GS_CONFIRM;
}

static void update_list(games_state_t *st)
{
    u32 rep = atlas_input_repeated();
    int n = atlas_game_count();

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        atlas_screen_pop();
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONTEXT)) {
        atlas_game_scan();
        st->cursor = 0;
        st->top = 0;
        return;
    }

    if (n == 0)
        return;

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + n - 1) % n;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % n;

    if (st->cursor < st->top)
        st->top = st->cursor;
    if (st->cursor >= st->top + games_visible())
        st->top = st->cursor - games_visible() + 1;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        choose(st);
}

static void update_confirm(games_state_t *st)
{
    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        st->mode = GS_LIST;
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        atlas_err_t err;

        /*
         * Nothing is written to the card first: unlike an application
         * launch there is no recently-used list for games, and a card
         * write here would be one more thing between the press and the
         * point where a failure can still be seen.
         *
         * Does not return when it works.
         */
        err = atlas_discboot_run(&st->ready);

        /*
         * It came back, so it refused. Two windows exist for that, and
         * only one still has a screen: ATLAS_EFATAL/ATLAS_EFAIL are
         * raised only after atlas_device_shutdown()/video shutdown
         * inside run(), so for those nothing can be drawn and the run
         * loop is asked to exit, same as before - main() tears down
         * what is left and the console returns to the browser rather
         * than sitting on a black screen with no way out but the
         * switch. Every other code (a stale BOOT2 path, an IOPBTCONF
         * this console's revision doesn't parse) is raised before that
         * point and is shown exactly like a prepare() failure would
         * have been.
         */
        if (err == ATLAS_EFATAL || err == ATLAS_EFAIL) {
            atlas_screen_request_exit();
            return;
        }

        st->mode = GS_FAILED;
        st->fail_reason = prepare_message(err);
    }
}

static void update_failed(games_state_t *st)
{
    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)
        || atlas_input_is_pressed(ATLAS_BTN_BACK))
        st->mode = GS_LIST;
}

static void games_update(atlas_screen_t *self)
{
    games_state_t *st = (games_state_t *)self->data;

    switch (st->mode) {
    case GS_CONFIRM: update_confirm(st); break;
    case GS_FAILED:  update_failed(st);  break;
    default:         update_list(st);    break;
    }
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

/*
 * The same layout the file manager's dialogs use, and for the same
 * reason: atlas_ui_message_box() draws one clipped line, and both of
 * the dialogs here are several - a warning that has been ellipsised
 * down to its first clause has stopped being a warning.
 */
static void draw_dialog(const char *title, const char **lines, int n,
                        const char *hint)
{
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float sh = (float)atlas_video_safe_h();
    float lh = atlas_ui_line_height();

    float w = sw * 0.84f;
    float h = lh * (4.5f + (float)n * 1.3f);
    float x = (sw - w) * 0.5f;
    float y = (sh - h) * 0.5f;
    float ty;
    int i;

    atlas_ui_rect(0.0f, 0.0f, sw, sh, ATLAS_RGBA(0x00, 0x00, 0x00, 0x50));

    atlas_ui_panel(x, y, w, h, t->panel);
    atlas_ui_rect(x, y, w, 3.0f, t->accent);

    ty = y + (float)ATLAS_UI_PAD;

    if (title) {
        atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, ty, t->text, title,
                              w - (float)ATLAS_UI_PAD * 2.0f);
        ty += lh * 1.6f;
    }

    for (i = 0; i < n; i++) {
        if (lines[i])
            atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, ty, t->text_dim,
                                  lines[i], w - (float)ATLAS_UI_PAD * 2.0f);
        ty += lh * 1.3f;
    }

    if (hint)
        atlas_ui_text(x + w * 0.5f, y + h - lh - (float)ATLAS_UI_PAD,
                      ATLAS_ALIGN_CENTER, t->text_dim, hint);
}

static void draw_confirm(const games_state_t *st)
{
    const char *lines[5];
    char ident[96];
    char hint[80];
    int n = 0;

    /*
     * What the image actually turned out to be, on its own line. The
     * filename is what the user chose; this is what the disc says it
     * is, and the two disagreeing is worth seeing before starting it.
     */
    snprintf(ident, sizeof(ident), "%s  [%s]  %s",
             st->ready.info.id,
             atlas_disc_region_str(st->ready.info.region),
             st->ready.has_compat ? st->ready.compat.id
                                  : atlas_str(ATLAS_STR_GAMES_NO_COMPAT));

    lines[n++] = ident;
    lines[n++] = st->ready.path;
    lines[n++] = atlas_str(ATLAS_STR_GAMES_WARN_1);
    lines[n++] = atlas_str(ATLAS_STR_GAMES_WARN_2);

    snprintf(hint, sizeof(hint), "X  %s     O  %s",
             atlas_str(ATLAS_STR_LAUNCH), atlas_str(ATLAS_STR_CANCEL));

    draw_dialog(atlas_str(ATLAS_STR_GAMES_WARN_TITLE), lines, n, hint);
}

static void draw_failed(const games_state_t *st)
{
    const atlas_game_t *g = atlas_game_get(st->cursor);
    const char *lines[2];
    char hint[32];
    int n = 0;

    lines[n++] = atlas_str(st->fail_reason);
    if (g)
        lines[n++] = g->path;

    snprintf(hint, sizeof(hint), "X  %s", atlas_str(ATLAS_STR_OK));

    draw_dialog(atlas_str(ATLAS_STR_GAMES_FAIL_TITLE), lines, n, hint);
}

static void draw_empty(float x, float y, float w)
{
    const atlas_theme_t *t = atlas_theme();
    float lh = atlas_ui_line_height();

    /*
     * Which of the two reasons applies matters: "no images" with a
     * stick plugged in means the folders are wrong, and without one it
     * means the stick is. Telling them apart is the difference between
     * a user copying a file and a user buying a new stick.
     */
    if (!atlas_device_is_ready(ATLAS_DEV_MASS) &&
        !atlas_device_is_ready(ATLAS_DEV_HDD)) {
        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text,
                      atlas_str(ATLAS_STR_GAMES_NO_USB));
        y += lh * 1.6f;

        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_GAMES_USB_ONLY), w);
        return;
    }

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text,
                  atlas_str(ATLAS_STR_GAMES_EMPTY));
    y += lh * 1.6f;

    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(ATLAS_STR_GAMES_EMPTY_HINT), w);
    y += lh * 1.4f;

    /* Left as they are: these are what has to be typed. */
    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                  "   mass:/            mass:/DVD/       mass:/CD/");
    y += lh;
    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                  "   mass:/ISO/        mass:/ATLAS/GAMES/");
    y += lh * 1.6f;

    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(ATLAS_STR_GAMES_USB_ONLY), w);
}

static void games_draw(atlas_screen_t *self)
{
    games_state_t *st = (games_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i, last, n = atlas_game_count();
    char hints[160];

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_GAMES));

    y = atlas_ui_content_y();
    y = atlas_ui_content_y();

    if (n == 0) {
        draw_empty(x, y, w);

        snprintf(hints, sizeof(hints), "Triangle  %s     O  %s",
                 atlas_str(ATLAS_STR_RESCAN), atlas_str(ATLAS_STR_BACK));
        atlas_ui_footer(hints);

        if (st->mode == GS_FAILED)
            draw_failed(st);
        return;
    }

    last = st->top + games_visible();
    if (last > n)
        last = n;

    for (i = st->top; i < last; i++) {
        const atlas_game_t *g = atlas_game_get(i);
        int selected = (i == st->cursor);
        float row_y;

        if (!g)
            continue;

        /* Only the cursor gets a slab; see the note in ui.c. */
        if (selected) {
            atlas_ui_panel(x, y, w, (float)ATLAS_UI_ROW_H,
                           t->panel_selected);
            atlas_ui_rect(x, y, 3.0f, (float)ATLAS_UI_ROW_H, t->accent);
        }

        row_y = y + ((float)ATLAS_UI_ROW_H - lh) * 0.5f;

        atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, row_y,
                              selected ? t->text : t->text_dim,
                              g->name,
                              w - (float)ATLAS_UI_PAD * 2.0f - 48.0f);

        /* An HDL row has no file extension - only a partition - so it
         * is tagged "HDD" instead. Everything else keeps the extension:
         * a ZSO and an ISO of the same title are two rows with the same
         * name otherwise. */
        if (g->is_hdl) {
            atlas_ui_text(x + w - (float)ATLAS_UI_PAD, row_y,
                          ATLAS_ALIGN_RIGHT, t->text_dim, "HDD");
        } else {
            const char *dot = strrchr(g->path, '.');

            if (dot)
                atlas_ui_text(x + w - (float)ATLAS_UI_PAD, row_y,
                              ATLAS_ALIGN_RIGHT, t->text_dim, dot + 1);
        }

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    if (n > games_visible()) {
        char pos[32];

        snprintf(pos, sizeof(pos), "%d / %d", st->cursor + 1, n);
        atlas_ui_text(x + w, y + (float)ATLAS_UI_ROW_GAP,
                      ATLAS_ALIGN_RIGHT, t->text_dim, pos);
    }

    {
        const atlas_game_t *g = atlas_game_get(st->cursor);

        if (g)
            atlas_ui_text_clipped(
                x,
                (float)(atlas_video_safe_h() - ATLAS_UI_FOOTER_H) - lh * 1.4f,
                t->text_dim, g->path, w);
    }

    snprintf(hints, sizeof(hints), "X  %s   Triangle  %s   O  %s",
             atlas_str(ATLAS_STR_LAUNCH), atlas_str(ATLAS_STR_RESCAN),
             atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);

    switch (st->mode) {
    case GS_CONFIRM: draw_confirm(st); break;
    case GS_FAILED:  draw_failed(st);  break;
    default: break;
    }
}

static atlas_screen_t s_screen = {
    "Games",
    games_enter,
    NULL,
    games_update,
    games_draw,
    &s_state
};

atlas_screen_t *atlas_screen_games(void)
{
    return &s_screen;
}
