/*
 * AtlasPS2 - screen_apps.c
 * The homebrew a scan found, and launching it.
 *
 * WHY THE LIST IS BUILT RATHER THAN DRAWN FROM THE CATALOGUE
 * ----------------------------------------------------------
 * With favorites and a recently-used list, one application can appear
 * in three places at once, and headings sit between the groups. So the
 * screen builds a flat array of rows first - headings and applications
 * mixed - and everything after that (cursor, scrolling, drawing) works
 * on rows. The alternative, three loops each carrying their own index
 * arithmetic, is where off-by-one bugs live.
 *
 * A user with no favorites and nothing launched yet gets exactly the
 * plain list they had before: the groups appear when there is
 * something to put in them.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/app.h"
#include "atlas/fav.h"
#include "atlas/launch.h"
#include "atlas/device.h"
#include "atlas/i18n.h"

/*
 * The list scrolls rather than paginates: a user pressing Down past the
 * bottom expects the next item, not a new page whose first row is the
 * one they were already on.
 */
#define APPS_VISIBLE 8

/*
 * Room for every application plus the three headings. The catalogue is
 * already bounded at ATLAS_APP_MAX, and favorites and recents can only
 * name applications that are in it, so this cannot overflow - but a
 * favorite listed and also shown under "All" is two rows, hence the
 * doubling.
 */
#define ROW_MAX (ATLAS_APP_MAX * 2 + ATLAS_RECENT_MAX + 4)

typedef struct {
    /* A heading carries a string id; an application carries its index
     * in the catalogue. -1 in `app` is what makes a row a heading. */
    atlas_str_id_t label;
    int            app;
} apps_row_t;

typedef struct {
    int cursor;
    int top;        /* first visible row */

    apps_row_t rows[ROW_MAX];
    int        row_count;

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
/* Building the row list                                               */
/* ------------------------------------------------------------------ */

static int row_is_heading(const apps_row_t *r)
{
    return r->app < 0;
}

/**
 * Which catalogue entry has this path, or -1.
 *
 * This is what "a path that no longer exists is kept and simply not
 * shown" means in practice: an unplugged USB stick makes its favorites
 * fail to resolve here, and they are skipped for as long as it is out.
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

static void row_add(apps_state_t *st, atlas_str_id_t label, int app)
{
    if (st->row_count >= ROW_MAX)
        return;

    st->rows[st->row_count].label = label;
    st->rows[st->row_count].app   = app;
    st->row_count++;
}

/**
 * Append a group, heading included, but only if it has any members.
 *
 * An empty "Favorites" heading is worse than no heading: it takes a
 * row from a 448-line screen to say nothing.
 */
static void add_group(apps_state_t *st, atlas_str_id_t heading,
                      int count, const char *(*get)(int))
{
    int i, first = 1;

    for (i = 0; i < count; i++) {
        int a = app_by_path(get(i));

        if (a < 0)
            continue;

        if (first) {
            row_add(st, heading, -1);
            first = 0;
        }

        row_add(st, heading, a);
    }
}

static void build_rows(apps_state_t *st)
{
    int i, n = atlas_app_count();
    int grouped;

    st->row_count = 0;

    add_group(st, ATLAS_STR_FAV_RECENT, atlas_recent_count(),
              atlas_recent_get);
    add_group(st, ATLAS_STR_FAV_FAVORITES, atlas_fav_count(),
              atlas_fav_get);

    grouped = st->row_count > 0;

    /*
     * The full list gets a heading only when something sits above it.
     * On a console with no favorites yet, "All applications" over a
     * list that is obviously all of them is noise.
     */
    if (grouped && n > 0)
        row_add(st, ATLAS_STR_FAV_ALL, -1);

    for (i = 0; i < n; i++)
        row_add(st, ATLAS_STR_FAV_ALL, i);
}

/** First selectable row at or after `from`, wrapping; -1 if none. */
static int next_app_row(const apps_state_t *st, int from, int step)
{
    int i, r = from;

    if (st->row_count <= 0)
        return -1;

    for (i = 0; i < st->row_count; i++) {
        r = (r + st->row_count) % st->row_count;
        if (!row_is_heading(&st->rows[r]))
            return r;
        r += step;
    }

    return -1;
}

/** The catalogue entry under the cursor, or NULL. */
static const atlas_app_t *selected_app(const apps_state_t *st)
{
    if (st->cursor < 0 || st->cursor >= st->row_count)
        return NULL;

    if (row_is_heading(&st->rows[st->cursor]))
        return NULL;

    return atlas_app_get(st->rows[st->cursor].app);
}

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

    build_rows(st);

    /* A rescan can shrink the list under a cursor left from last time. */
    if (st->cursor >= st->row_count)
        st->cursor = 0;
    if (st->cursor < 0)
        st->cursor = 0;

    st->cursor = next_app_row(st, st->cursor, 1);
    if (st->cursor < 0)
        st->cursor = 0;

    st->top = 0;
}

/*
 * Leaving is where a card write is affordable: the screen is changing
 * anyway, so the pause is one the user already expects. Nothing is
 * written unless a star was actually toggled.
 */
static void apps_leave(atlas_screen_t *self)
{
    (void)self;
    atlas_fav_save();
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
        build_rows(st);
        st->cursor = next_app_row(st, 0, 1);
        if (st->cursor < 0)
            st->cursor = 0;
        st->top = 0;
        return;
    }

    if (st->row_count == 0)
        return;

    if (rep & ATLAS_BTN_UP)
        st->cursor = next_app_row(st, st->cursor - 1, -1);

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = next_app_row(st, st->cursor + 1, 1);

    if (st->cursor < 0)
        st->cursor = 0;

    /*
     * Keep the cursor inside the window, following it at the edges,
     * and pull the heading above it into view when there is one: a
     * favorite scrolled to the top of the screen with its heading just
     * off it looks like an application in the wrong group.
     */
    if (st->cursor < st->top)
        st->top = st->cursor;
    if (st->cursor >= st->top + APPS_VISIBLE)
        st->top = st->cursor - APPS_VISIBLE + 1;
    if (st->top > 0 && st->top == st->cursor
        && row_is_heading(&st->rows[st->top - 1]))
        st->top--;

    if (atlas_input_is_pressed(ATLAS_BTN_ACTION)) {
        const atlas_app_t *a = selected_app(st);

        if (a) {
            atlas_fav_toggle(a->path);
            /* The groups changed under the cursor. Rebuilding here and
             * keeping the application selected is why the row list is
             * rebuilt from paths rather than patched in place. */
            {
                char path[ATLAS_APP_PATH_MAX];
                int r;

                snprintf(path, sizeof(path), "%s", a->path);
                build_rows(st);

                for (r = 0; r < st->row_count; r++) {
                    const atlas_app_t *b;

                    if (row_is_heading(&st->rows[r]))
                        continue;

                    b = atlas_app_get(st->rows[r].app);
                    if (b && strcmp(b->path, path) == 0) {
                        st->cursor = r;
                        break;
                    }
                }
            }
        }
        return;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        const atlas_app_t *a = selected_app(st);

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

            /*
             * Recorded and flushed before the handover, not after:
             * after, this program has been replaced and there is no
             * "after" to write anything in.
             */
            atlas_recent_note(a->path);
            atlas_fav_save();

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
    int i, last;
    char hints[160];

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_APPS));

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_APPS));
    y += lh * 2.2f;

    if (st->row_count == 0) {
        draw_empty(x, y, w);

        snprintf(hints, sizeof(hints), "Triangle  %s     O  %s",
                 atlas_str(ATLAS_STR_RESCAN), atlas_str(ATLAS_STR_BACK));
        atlas_ui_footer(hints);

        if (st->failed)
            draw_failure(st);
        return;
    }

    last = st->top + APPS_VISIBLE;
    if (last > st->row_count)
        last = st->row_count;

    for (i = st->top; i < last; i++) {
        const apps_row_t *r = &st->rows[i];
        const atlas_app_t *a;
        int selected;
        float row_y;

        /*
         * A heading is drawn without a panel behind it, at the dim
         * colour, and takes a shorter row: it is a label, and giving
         * it the same slab as an application makes it look like one
         * more thing to press X on.
         */
        if (row_is_heading(r)) {
            atlas_ui_text(x + (float)ATLAS_UI_PAD * 0.5f,
                          y + ((float)ATLAS_UI_ROW_H - lh) * 0.5f,
                          ATLAS_ALIGN_LEFT, t->accent,
                          atlas_str(r->label));
            y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
            continue;
        }

        a = atlas_app_get(r->app);
        if (!a)
            continue;

        selected = (i == st->cursor);

        atlas_ui_panel(x, y, w, (float)ATLAS_UI_ROW_H,
                       selected ? t->panel_selected : t->panel);

        row_y = y + ((float)ATLAS_UI_ROW_H - lh) * 0.5f;

        /*
         * The name is clipped to leave the star and the device tag
         * room. Which device an application is on is what tells two
         * copies of the same homebrew apart, so it must not be the
         * part that gets pushed off the row.
         */
        atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, row_y,
                              selected ? t->text : t->text_dim,
                              a->name,
                              w - (float)ATLAS_UI_PAD * 2.0f - 72.0f);

        atlas_ui_text(x + w - (float)ATLAS_UI_PAD, row_y,
                      ATLAS_ALIGN_RIGHT, t->text_dim,
                      device_short(a->device));

        /* An asterisk rather than a glyph: the font atlas is baked
         * from DejaVu Sans over Latin-1, and a star is not in it. */
        if (atlas_fav_is(a->path))
            atlas_ui_text(x + w - (float)ATLAS_UI_PAD - 40.0f, row_y,
                          ATLAS_ALIGN_RIGHT, t->accent, "*");

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    /* Position, not a scrollbar: at 640x448 a bar thin enough to look
     * right is one pixel wide and flickers on an interlaced CRT. */
    if (st->row_count > APPS_VISIBLE) {
        char pos[32];

        snprintf(pos, sizeof(pos), "%d / %d", st->cursor + 1,
                 st->row_count);
        atlas_ui_text(x + w, y + (float)ATLAS_UI_ROW_GAP,
                      ATLAS_ALIGN_RIGHT, t->text_dim, pos);
    }

    /* The selected application's path, so the user can tell two
     * identically named copies apart before launching one. */
    {
        const atlas_app_t *a = selected_app(st);

        if (a)
            atlas_ui_text_clipped(
                x,
                (float)(atlas_video_safe_h() - ATLAS_UI_FOOTER_H) - lh * 1.4f,
                t->text_dim, a->path, w);
    }

    snprintf(hints, sizeof(hints),
             "X  %s   Square  %s   Triangle  %s   O  %s",
             atlas_str(ATLAS_STR_LAUNCH), atlas_str(ATLAS_STR_FAV_TOGGLE),
             atlas_str(ATLAS_STR_RESCAN), atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);

    if (st->failed)
        draw_failure(st);
}

static atlas_screen_t s_screen = {
    "Applications",
    apps_enter,
    apps_leave,
    apps_update,
    apps_draw,
    &s_state
};

atlas_screen_t *atlas_screen_apps(void)
{
    return &s_screen;
}
