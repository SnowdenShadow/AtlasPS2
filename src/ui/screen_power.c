/*
 * AtlasPS2 - screen_power.c
 * The power menu.
 */
#include <string.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/power.h"

/*
 * Entries are built at enter() time rather than being a fixed table,
 * because the spec requires that only actions that can actually be
 * performed appear: offering "Power Off" on a console whose poweroff
 * module failed would be a button that silently does nothing.
 */

typedef enum {
    ACT_BROWSER = 0,
    ACT_SHUTDOWN,
    ACT_CANCEL,
    ACT_COUNT
} power_action_t;

typedef struct {
    power_action_t action;
    const char *label;
    const char *detail;
} power_entry_t;

typedef struct {
    power_entry_t entries[ACT_COUNT];
    int count;
    int cursor;
    int confirming;
} power_state_t;

static power_state_t s_state;

static void power_enter(atlas_screen_t *self)
{
    power_state_t *st = (power_state_t *)self->data;
    int n = 0;

    st->entries[n].action = ACT_BROWSER;
    st->entries[n].label  = "Return to PS2 Browser";
    st->entries[n].detail = "Leave AtlasPS2 and start the console browser.";
    n++;

    if (atlas_power_can_shutdown()) {
        st->entries[n].action = ACT_SHUTDOWN;
        st->entries[n].label  = "Power Off";
        st->entries[n].detail = "Shut the console down.";
        n++;
    }

    st->entries[n].action = ACT_CANCEL;
    st->entries[n].label  = "Cancel";
    st->entries[n].detail = NULL;
    n++;

    st->count = n;

    /* Start on Cancel: the least destructive default. */
    st->cursor = n - 1;
    st->confirming = 0;
}

static void perform(power_action_t action)
{
    switch (action) {
    case ACT_BROWSER:
        atlas_power_exit_to_browser();
        break;

    case ACT_SHUTDOWN:
        atlas_power_shutdown();
        break;

    case ACT_CANCEL:
    default:
        atlas_screen_pop();
        break;
    }

    /*
     * Both real actions are noreturn on success. Reaching here means
     * one failed, so the menu stays up and the user can pick again
     * rather than facing a frozen screen.
     */
}

static void power_update(atlas_screen_t *self)
{
    power_state_t *st = (power_state_t *)self->data;
    u32 rep = atlas_input_repeated();

    if (st->confirming) {
        /*
         * Leaving AtlasPS2 or cutting power is not undoable, so it takes
         * a second, deliberate press. Back cancels.
         */
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
            perform(st->entries[st->cursor].action);
        else if (atlas_input_is_pressed(ATLAS_BTN_BACK))
            st->confirming = 0;

        return;
    }

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + st->count - 1) % st->count;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % st->count;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        if (st->entries[st->cursor].action == ACT_CANCEL)
            atlas_screen_pop();
        else
            st->confirming = 1;
    }

    if (atlas_input_is_pressed(ATLAS_BTN_BACK))
        atlas_screen_pop();
}

static void power_draw(atlas_screen_t *self)
{
    power_state_t *st = (power_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float row_w = sw * 0.62f;
    float x = (float)ATLAS_UI_PAD;
    float y;
    int i;

    atlas_ui_header("Power");

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text, "Power");
    y += atlas_ui_line_height() * 2.2f;

    for (i = 0; i < st->count; i++) {
        atlas_ui_menu_row(x, y, row_w, i == st->cursor,
                          st->entries[i].label, NULL);
        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    if (st->entries[st->cursor].detail) {
        y += atlas_ui_line_height() * 0.5f;
        atlas_ui_text_clipped(x, y, t->text_dim,
                              st->entries[st->cursor].detail,
                              sw - (float)ATLAS_UI_PAD * 2.0f);
    }

    atlas_ui_footer("X  Select     O  Back");

    if (st->confirming)
        atlas_ui_message_box(st->entries[st->cursor].label,
                             st->entries[st->cursor].detail,
                             "X  Confirm     O  Cancel");
}

static atlas_screen_t s_screen = {
    "Power",
    power_enter,
    NULL,
    power_update,
    power_draw,
    &s_state
};

atlas_screen_t *atlas_screen_power(void)
{
    return &s_screen;
}
