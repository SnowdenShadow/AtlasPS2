/*
 * AtlasPS2 - screen_apps.c
 * The homebrew a scan found, and launching it.
 */
#include <stdio.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/app.h"
#include "atlas/launch.h"
#include "atlas/device.h"
#include "atlas/i18n.h"

/*
 * The list scrolls rather than paginates: a user pressing Down past the
 * bottom expects the next item, not a new page whose first row is the
 * one they were already on.
 */
#define APPS_VISIBLE 8

typedef struct {
    int cursor;
    int top;        /* first visible row */

    /*
     * A launch failure is the one message this screen must not lose:
     * the alternative is the user pressing X repeatedly at a row that
     * silently does nothing. It stays up until dismissed.
     */
    int            failed;
    atlas_str_id_t fail_reason;
} apps_state_t;

static apps_state_t s_state;

/* ------------------------------------------------------------------ */
/* Scanning                                                            */
/* ------------------------------------------------------------------ */

/*
 * Scanning walks directories on a Memory Card and costs a visible
 * fraction of a second, so it happens on entering the screen and on an
 * explicit Triangle - never per frame, and never from draw.
 */
static void apps_enter(atlas_screen_t *self)
{
    apps_state_t *st = (apps_state_t *)self->data;

    st->failed = 0;
    st->fail_reason = ATLAS_STR_APPS_FAIL_OTHER;

    if (!atlas_app_scanned())
        atlas_app_scan();

    /* A rescan can shrink the list under a cursor left from last time. */
    if (st->cursor >= atlas_app_count())
        st->cursor = 0;
    if (st->cursor < 0)
        st->cursor = 0;

    st->top = 0;
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

/*
 * The reason is kept as a key rather than as resolved text: the box can
 * stay up across a language change, and a half-translated error is the
 * last thing a user should have to read.
 */
static atlas_str_id_t launch_message(atlas_err_t err)
{
    switch (err) {
    case ATLAS_ENOENT:  return ATLAS_STR_APPS_FAIL_GONE;
    case ATLAS_EFORMAT: return ATLAS_STR_APPS_FAIL_FORMAT;
    default:            return ATLAS_STR_APPS_FAIL_OTHER;
    }
}

static void apps_update(atlas_screen_t *self)
{
    apps_state_t *st = (apps_state_t *)self->data;
    u32 rep = atlas_input_repeated();
    int count = atlas_app_count();

    /* The failure box owns input while it is up: any button dismisses
     * it, and nothing else happens on that press. */
    if (st->failed) {
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)
            || atlas_input_is_pressed(ATLAS_BTN_BACK))
            st->failed = 0;
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
        atlas_screen_pop();
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONTEXT)) {
        atlas_app_scan();
        st->cursor = 0;
        st->top = 0;
        return;
    }

    if (count == 0)
        return;

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + count - 1) % count;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % count;

    /* Keep the cursor inside the window, following it at the edges. */
    if (st->cursor < st->top)
        st->top = st->cursor;
    if (st->cursor >= st->top + APPS_VISIBLE)
        st->top = st->cursor - APPS_VISIBLE + 1;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        const atlas_app_t *a = atlas_app_get(st->cursor);

        if (a) {
            /*
             * Checked before the handover, while there is still a
             * screen to report on. atlas_launch_elf() re-checks, but by
             * then our video is down and a failure has nowhere to go.
             */
            atlas_err_t err = atlas_launch_check(a->path);

            if (err != ATLAS_OK) {
                st->failed = 1;
                st->fail_reason = launch_message(err);
                return;
            }

            /* Does not return if it works. */
            err = atlas_launch_elf(a->path, 0, NULL);

            /*
             * It came back, which means the loader refused after our
             * subsystems were shut down. Nothing can be drawn from
             * here - ask the run loop to exit so main() can tear down
             * what is left and the console returns to the browser
             * rather than sitting on a black screen.
             */
            atlas_screen_request_exit();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static const char *device_short(atlas_device_id_t id)
{
    switch (id) {
    case ATLAS_DEV_MC0:  return "MC1";
    case ATLAS_DEV_MC1:  return "MC2";
    case ATLAS_DEV_MASS: return "USB";
    case ATLAS_DEV_HDD:  return "HDD";
    default:             return "";
    }
}

static void draw_empty(float x, float y, float w)
{
    const atlas_theme_t *t = atlas_theme();
    float lh = atlas_ui_line_height();

    /*
     * "Nothing found" without saying where we looked leaves the user
     * with no next step. The paths are the answer to the question they
     * are about to ask.
     */
    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text,
                  atlas_str(ATLAS_STR_APPS_EMPTY));
    y += lh * 1.6f;

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                  atlas_str(ATLAS_STR_APPS_EMPTY_HINT));
    y += lh * 1.4f;

    /* The paths stay as they are: they are what has to be typed. */

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                  "   mc0:/ATLAS/APPS/      mc1:/ATLAS/APPS/");
    y += lh;
    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                  "   mass:/APPS/           mass:/ATLAS/APPS/");
    y += lh * 1.6f;

    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(ATLAS_STR_APPS_EMPTY_META), w);
}

static void draw_failure(const apps_state_t *st)
{
    char ok[32];

    snprintf(ok, sizeof(ok), "X  %s", atlas_str(ATLAS_STR_OK));
    atlas_ui_message_box(atlas_str(ATLAS_STR_APPS_FAIL_TITLE),
                         atlas_str(st->fail_reason), ok);
}

static void apps_draw(atlas_screen_t *self)
{
    apps_state_t *st = (apps_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int count = atlas_app_count();
    int i, last;
    char hints[128];

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_APPS));

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_APPS));
    y += lh * 2.2f;

    if (count == 0) {
        draw_empty(x, y, w);

        snprintf(hints, sizeof(hints), "Triangle  %s     O  %s",
                 atlas_str(ATLAS_STR_RESCAN), atlas_str(ATLAS_STR_BACK));
        atlas_ui_footer(hints);

        if (st->failed)
            draw_failure(st);
        return;
    }

    last = st->top + APPS_VISIBLE;
    if (last > count)
        last = count;

    for (i = st->top; i < last; i++) {
        const atlas_app_t *a = atlas_app_get(i);
        int selected = (i == st->cursor);
        float row_y;

        if (!a)
            continue;

        atlas_ui_panel(x, y, w, (float)ATLAS_UI_ROW_H,
                       selected ? t->panel_selected : t->panel);

        row_y = y + ((float)ATLAS_UI_ROW_H - lh) * 0.5f;

        /*
         * The name is clipped to leave the device tag room. Which
         * device an application is on is what tells two copies of the
         * same homebrew apart, so it must not be the part that gets
         * pushed off the row.
         */
        atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, row_y,
                              selected ? t->text : t->text_dim,
                              a->name,
                              w - (float)ATLAS_UI_PAD * 2.0f - 48.0f);

        atlas_ui_text(x + w - (float)ATLAS_UI_PAD, row_y,
                      ATLAS_ALIGN_RIGHT, t->text_dim,
                      device_short(a->device));

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    /* Position, not a scrollbar: at 640x448 a bar thin enough to look
     * right is one pixel wide and flickers on an interlaced CRT. */
    if (count > APPS_VISIBLE) {
        char pos[32];

        snprintf(pos, sizeof(pos), "%d / %d", st->cursor + 1, count);
        atlas_ui_text(x + w, y + (float)ATLAS_UI_ROW_GAP,
                      ATLAS_ALIGN_RIGHT, t->text_dim, pos);
    }

    /* The selected application's path, so the user can tell two
     * identically named copies apart before launching one. */
    {
        const atlas_app_t *a = atlas_app_get(st->cursor);

        if (a)
            atlas_ui_text_clipped(
                x,
                (float)(atlas_video_safe_h() - ATLAS_UI_FOOTER_H) - lh * 1.4f,
                t->text_dim, a->path, w);
    }

    snprintf(hints, sizeof(hints), "X  %s     Triangle  %s     O  %s",
             atlas_str(ATLAS_STR_LAUNCH), atlas_str(ATLAS_STR_RESCAN),
             atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);

    if (st->failed)
        draw_failure(st);
}

static atlas_screen_t s_screen = {
    "Applications",
    apps_enter,
    NULL,
    apps_update,
    apps_draw,
    &s_state
};

atlas_screen_t *atlas_screen_apps(void)
{
    return &s_screen;
}
