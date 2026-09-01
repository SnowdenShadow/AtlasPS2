/*
 * AtlasPS2 - screen_devices.c
 * What storage the console can see right now.
 */
#include <stdio.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/device.h"
#include "atlas/i18n.h"

/*
 * The first place a user looks when a Memory Card or a stick does not
 * show up in the file manager. So it reports the *reason* a device is
 * unusable, not just that it is: "not formatted" and "not a PS2 card"
 * are things the user can act on, while a missing row is not.
 *
 * A USB stick can take seconds to enumerate after power-on, so this
 * screen keeps polling while it is open and rows change under the
 * cursor as devices appear.
 */

static const char *state_label(const atlas_device_t *d)
{
    switch (d->state) {
    case ATLAS_DEV_READY:       return atlas_str(ATLAS_STR_DEV_READY);
    case ATLAS_DEV_UNFORMATTED: return atlas_str(ATLAS_STR_DEV_UNFORMATTED);
    case ATLAS_DEV_ERROR:       return atlas_str(ATLAS_STR_DEV_ERROR);
    default:                    return atlas_str(ATLAS_STR_DEV_ABSENT);
    }
}

static u64 state_color(const atlas_device_t *d)
{
    const atlas_theme_t *t = atlas_theme();

    switch (d->state) {
    case ATLAS_DEV_READY:       return t->ok;
    case ATLAS_DEV_UNFORMATTED: return t->warn;
    case ATLAS_DEV_ERROR:       return t->error;
    default:                    return t->text_dim;
    }
}

static void devices_update(atlas_screen_t *self)
{
    /*
     * No poll here: the frame loop already refreshes one device per
     * frame for every screen, since the header indicators are drawn
     * everywhere. Polling again would only skip devices in the cycle.
     */
    if (atlas_input_is_pressed(ATLAS_BTN_BACK))
        atlas_screen_pop();
}

static void devices_draw(atlas_screen_t *self)
{
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i;

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_DEVICES));

    y = atlas_ui_content_y();
    y = atlas_ui_content_y();

    for (i = 0; i < ATLAS_DEV_COUNT; i++) {
        const atlas_device_t *d = atlas_device_get((atlas_device_id_t)i);
        float row_y;

        if (!d)
            continue;

        /*
         * A rule under each row rather than a slab behind it. This
         * screen has no cursor - it is a table you read, not a list you
         * move through - and a slab is the interface's way of saying
         * "this one is selected". Drawing one behind every row says it
         * about all of them, which is the same as saying nothing.
         */
        row_y = y + ((float)ATLAS_UI_ROW_H - lh) * 0.5f;

        atlas_ui_text(x + (float)ATLAS_UI_PAD, row_y, ATLAS_ALIGN_LEFT,
                      t->text, d->name);

        atlas_ui_text(x + w - (float)ATLAS_UI_PAD, row_y, ATLAS_ALIGN_RIGHT,
                      state_color(d), state_label(d));

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);

        /*
         * The detail line sits under its row rather than in it: an
         * error message is longer than the space a right-aligned value
         * has, and truncating the one thing that explains the problem
         * would defeat the point of the screen.
         */
        if (d->state == ATLAS_DEV_READY) {
            char buf[96];
            char amount[48];

            /* The free-space phrase is one translated string with a %d
             * in it, not "%d" plus a word: French puts the number in a
             * different place relative to the unit, and splitting it
             * would make that impossible to express. */
            if (d->free_kb >= 0)
                snprintf(amount, sizeof(amount),
                         atlas_str(ATLAS_STR_DEV_FREE_KB), d->free_kb);
            else
                snprintf(amount, sizeof(amount), "%s",
                         atlas_str(ATLAS_STR_DEV_FREE_UNKNOWN));

            snprintf(buf, sizeof(buf), "%s   %s", d->path, amount);

            atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, y, t->text_dim,
                                  buf, w - (float)ATLAS_UI_PAD * 2.0f);
            y += lh + (float)ATLAS_UI_ROW_GAP;
        } else if (d->detail) {
            atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, y, t->text_dim,
                                  d->detail, w - (float)ATLAS_UI_PAD * 2.0f);
            y += lh + (float)ATLAS_UI_ROW_GAP;
        }

        /* The rule goes after the detail line, so it separates devices
         * rather than cutting one in half. */
        atlas_ui_separator(x, y, w, t->separator);
        y += (float)ATLAS_UI_ROW_GAP * 2.0f;
    }

    {
        char hints[64];
        snprintf(hints, sizeof(hints), "O  %s", atlas_str(ATLAS_STR_BACK));
        atlas_ui_footer(hints);
    }
}

static atlas_screen_t s_screen = {
    "Devices",
    NULL, NULL,
    devices_update,
    devices_draw,
    NULL
};

atlas_screen_t *atlas_screen_devices(void)
{
    return &s_screen;
}
