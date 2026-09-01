/*
 * AtlasPS2 - screen_todo.c
 * Placeholder for a screen a later milestone will implement.
 */
#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/i18n.h"

#include <stdio.h>

/*
 * A menu entry that opens a black screen is indistinguishable from a
 * crash. This says what the screen will do and how to get back, so a
 * release that is mid-roadmap is still honest and still navigable.
 *
 * One shared instance: only one placeholder is ever on the stack, since
 * a placeholder has nothing to push.
 */

typedef struct {
    const char *title;
    const char *detail;
} todo_state_t;

static todo_state_t s_state;

static void todo_update(atlas_screen_t *self)
{
    if (atlas_input_is_pressed(ATLAS_BTN_BACK)
        || atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        atlas_screen_pop();
}

static void todo_draw(atlas_screen_t *self)
{
    todo_state_t *st = (todo_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float y;

    atlas_ui_header(st->title);

    y = atlas_ui_content_y();
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text, st->title);
    y = atlas_ui_content_y_titled();

    if (st->detail) {
        atlas_ui_text_clipped(x, y, t->text_dim, st->detail, w);
        y += atlas_ui_line_height() * 1.6f;
    }

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->warn,
                  atlas_str(ATLAS_STR_TODO_BODY));

    {
        char hints[64];
        snprintf(hints, sizeof(hints), "O  %s", atlas_str(ATLAS_STR_BACK));
        atlas_ui_footer(hints);
    }
}

static atlas_screen_t s_screen = {
    "Placeholder",
    NULL, NULL,
    todo_update,
    todo_draw,
    &s_state
};

atlas_screen_t *atlas_screen_todo(const char *title, const char *detail)
{
    s_state.title = title ? title : atlas_str(ATLAS_STR_TODO_TITLE);
    s_state.detail = detail;

    /* The screen's displayed name follows whatever opened it. */
    s_screen.name = s_state.title;

    return &s_screen;
}
