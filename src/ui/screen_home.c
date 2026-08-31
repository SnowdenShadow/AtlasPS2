/*
 * AtlasPS2 - screen_home.c
 * The root menu.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/boot.h"
#include "atlas/atlas.h"

/* ------------------------------------------------------------------ */
/* Entries                                                             */
/*                                                                     */
/* Ordered by how often they are used, not by the order the spec lists */
/* them: the first row is where the cursor starts and needs the        */
/* fewest presses to reach.                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *label;
    const char *detail; /* shown by the placeholder screen */
} home_entry_t;

static const home_entry_t s_entries[] = {
    { "Games",        "Browse and launch games from your devices." },
    { "Applications", "Homebrew found on MC0, MC1, USB and HDD." },
    { "File Manager", "Copy, move and delete files across devices." },
    { "Video",        "Display mode, aspect ratio and screen position." },
    { "Settings",     "Language, controls, devices and updates." },
    { "System Info",  NULL },
    { "Power",        NULL }
};

#define HOME_COUNT ((int)(sizeof(s_entries) / sizeof(s_entries[0])))

typedef struct {
    int cursor;
} home_state_t;

static home_state_t s_state;

/* ------------------------------------------------------------------ */
/* Device indicators                                                   */
/* ------------------------------------------------------------------ */

/**
 * Draw one device indicator: a label with a lit or unlit dot.
 *
 * The dot is a primitive rather than a glyph because the font atlas
 * covers Latin-1 only - a bullet character would decode past U+00FF and
 * render as '?'. It also means the indicator picks up the theme's
 * colours instead of being stuck at the text colour.
 *
 * @return the x at which the next indicator should start.
 */
static float draw_indicator(float x, float y, const char *label, int online)
{
    const atlas_theme_t *t = atlas_theme();
    const float dot = 6.0f;
    float lw = atlas_ui_text_width(label);

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->bar_text, label);

    /* Centre the dot on the text's x-height rather than its box. */
    atlas_ui_rect(x + lw + 5.0f,
                  y + (atlas_ui_line_height() - dot) * 0.5f,
                  dot, dot,
                  online ? t->ok : t->separator);

    return x + lw + dot + 16.0f;
}

/**
 * Milestone 2 reports what the IOP module status implies rather than
 * probing the devices: the device layer does not exist yet, and the
 * spec is explicit that polling devices must never stall the UI. When
 * atlas_device_* lands this reads its cache, refreshed off the draw
 * path.
 */
static void draw_indicators(float right_edge, float y)
{
    const atlas_boot_status_t *st = atlas_boot_status();
    float w = atlas_ui_text_width("MC") + atlas_ui_text_width("USB")
            + 6.0f * 2.0f + 5.0f * 2.0f + 16.0f;
    float x = right_edge - w;

    x = draw_indicator(x, y, "MC", st->memcard);
    draw_indicator(x, y, "USB", st->usb);
}

/* ------------------------------------------------------------------ */
/* Screen                                                              */
/* ------------------------------------------------------------------ */

static void home_update(atlas_screen_t *self)
{
    home_state_t *st = (home_state_t *)self->data;
    u32 rep = atlas_input_repeated();

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + HOME_COUNT - 1) % HOME_COUNT;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % HOME_COUNT;

    /*
     * Confirm uses pressed(), not repeated(): a held Cross must open one
     * screen, not open and immediately re-open it.
     */
    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        const home_entry_t *e = &s_entries[st->cursor];

        if (strcmp(e->label, "System Info") == 0)
            atlas_screen_push(atlas_screen_sysinfo());
        else if (strcmp(e->label, "Power") == 0)
            atlas_screen_push(atlas_screen_power());
        else
            atlas_screen_push(atlas_screen_todo(e->label, e->detail));
    }

    if (atlas_input_is_pressed(ATLAS_BTN_SELECT))
        atlas_screen_push(atlas_screen_sysinfo());

    if (atlas_input_is_pressed(ATLAS_BTN_START))
        atlas_screen_push(atlas_screen_power());
}

static void home_draw(atlas_screen_t *self)
{
    home_state_t *st = (home_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float row_w = sw * 0.62f;
    float x = (float)ATLAS_UI_PAD;
    float y;
    int i;

    atlas_ui_header(NULL);
    draw_indicators(sw - (float)ATLAS_UI_PAD,
                    ((float)ATLAS_UI_HEADER_H - atlas_ui_line_height()) * 0.5f);

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;

    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text, "Welcome");
    y += atlas_ui_line_height() * 2.2f;

    for (i = 0; i < HOME_COUNT; i++) {
        atlas_ui_menu_row(x, y, row_w, i == st->cursor,
                          s_entries[i].label, NULL);
        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    atlas_ui_footer("X  Select     SELECT  System Info     START  Power");
}

static atlas_screen_t s_screen = {
    "Home",
    NULL,          /* enter: the cursor persists between visits */
    NULL,          /* leave */
    home_update,
    home_draw,
    &s_state
};

atlas_screen_t *atlas_screen_home(void)
{
    return &s_screen;
}
