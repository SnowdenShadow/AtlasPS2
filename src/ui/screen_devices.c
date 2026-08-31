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
    case ATLAS_DEV_READY:       return "Ready";
    case ATLAS_DEV_UNFORMATTED: return "Not formatted";
    case ATLAS_DEV_ERROR:       return "Error";
    default:                    return "Not connected";
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

    atlas_ui_header("Devices");

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text, "Devices");
    y += lh * 2.2f;

    for (i = 0; i < ATLAS_DEV_COUNT; i++) {
        const atlas_device_t *d = atlas_device_get((atlas_device_id_t)i);
        float row_y;

        if (!d)
            continue;

        atlas_ui_panel(x, y, w, (float)ATLAS_UI_ROW_H, t->panel);

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
            char buf[64];

            if (d->free_kb >= 0)
                snprintf(buf, sizeof(buf), "%s   %d KB free",
                         d->path, d->free_kb);
            else
                snprintf(buf, sizeof(buf), "%s   free space unknown",
                         d->path);

            atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, y, t->text_dim,
                                  buf, w - (float)ATLAS_UI_PAD * 2.0f);
            y += lh + (float)ATLAS_UI_ROW_GAP;
        } else if (d->detail) {
            atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, y, t->text_dim,
                                  d->detail, w - (float)ATLAS_UI_PAD * 2.0f);
            y += lh + (float)ATLAS_UI_ROW_GAP;
        }
    }

    atlas_ui_footer("O  Back");
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
