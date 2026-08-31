/*
 * AtlasPS2 - screen_home.c
 * The root menu.
 */
#include <stdio.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/boot.h"
#include "atlas/device.h"
#include "atlas/atlas.h"
#include "atlas/i18n.h"

/* ------------------------------------------------------------------ */
/* Entries                                                             */
/*                                                                     */
/* Ordered by how often they are used, not by the order the spec lists */
/* them: the first row is where the cursor starts and needs the        */
/* fewest presses to reach.                                            */
/* ------------------------------------------------------------------ */

/*
 * Each row carries what it opens, rather than the dispatcher matching on
 * the label. A translated menu is exactly the case where comparing text
 * would stop working - and it would stop working silently, leaving the
 * French build with rows that open the placeholder instead of the
 * screen they name.
 */
typedef enum {
    HOME_GO_TODO = 0,
    HOME_GO_APPS,
    HOME_GO_DEVICES,
    HOME_GO_VIDEO,
    HOME_GO_SYSINFO,
    HOME_GO_POWER
} home_target_t;

typedef struct {
    home_target_t  target;
    atlas_str_id_t label;
    atlas_str_id_t detail;  /* ATLAS_STR_COUNT for none; placeholder only */
} home_entry_t;

static const home_entry_t s_entries[] = {
    { HOME_GO_TODO,    ATLAS_STR_HOME_GAMES,    ATLAS_STR_HOME_D_GAMES    },
    { HOME_GO_APPS,    ATLAS_STR_HOME_APPS,     ATLAS_STR_COUNT           },
    { HOME_GO_TODO,    ATLAS_STR_HOME_FILES,    ATLAS_STR_HOME_D_FILES    },
    { HOME_GO_DEVICES, ATLAS_STR_HOME_DEVICES,  ATLAS_STR_COUNT           },
    { HOME_GO_VIDEO,   ATLAS_STR_HOME_VIDEO,    ATLAS_STR_COUNT           },
    { HOME_GO_TODO,    ATLAS_STR_HOME_SETTINGS, ATLAS_STR_HOME_D_SETTINGS },
    { HOME_GO_SYSINFO, ATLAS_STR_HOME_SYSINFO,  ATLAS_STR_COUNT           },
    { HOME_GO_POWER,   ATLAS_STR_HOME_POWER,    ATLAS_STR_COUNT           }
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
 * Reads the device layer's cache, which the update half of the frame
 * refreshes. Nothing here probes hardware: a draw path that blocks on a
 * Memory Card slot turns a 60 Hz interface into a stuttering one.
 *
 * Either slot lights the MC dot, since the label covers both.
 */
static void draw_indicators(float right_edge, float y)
{
    float w = atlas_ui_text_width("MC") + atlas_ui_text_width("USB")
            + 6.0f * 2.0f + 5.0f * 2.0f + 16.0f;
    float x = right_edge - w;
    int mc = atlas_device_is_ready(ATLAS_DEV_MC0)
          || atlas_device_is_ready(ATLAS_DEV_MC1);

    x = draw_indicator(x, y, "MC", mc);
    draw_indicator(x, y, "USB", atlas_device_is_ready(ATLAS_DEV_MASS));
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

        switch (e->target) {
        case HOME_GO_APPS:
            atlas_screen_push(atlas_screen_apps());
            break;
        case HOME_GO_DEVICES:
            atlas_screen_push(atlas_screen_devices());
            break;
        case HOME_GO_VIDEO:
            atlas_screen_push(atlas_screen_video());
            break;
        case HOME_GO_SYSINFO:
            atlas_screen_push(atlas_screen_sysinfo());
            break;
        case HOME_GO_POWER:
            atlas_screen_push(atlas_screen_power());
            break;
        case HOME_GO_TODO:
        default:
            /*
             * The placeholder is handed resolved text, not keys: it is
             * the one screen whose title can also come from a caller
             * that has no string id to give.
             */
            atlas_screen_push(atlas_screen_todo(
                atlas_str(e->label),
                e->detail != ATLAS_STR_COUNT ? atlas_str(e->detail) : NULL));
            break;
        }
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
    char hints[128];

    atlas_ui_header(NULL);
    draw_indicators(sw - (float)ATLAS_UI_PAD,
                    ((float)ATLAS_UI_HEADER_H - atlas_ui_line_height()) * 0.5f);

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;

    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_WELCOME));
    y += atlas_ui_line_height() * 2.2f;

    for (i = 0; i < HOME_COUNT; i++) {
        atlas_ui_menu_row(x, y, row_w, i == st->cursor,
                          atlas_str(s_entries[i].label), NULL);
        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    snprintf(hints, sizeof(hints), "X  %s     SELECT  %s     START  %s",
             atlas_str(ATLAS_STR_SELECT), atlas_str(ATLAS_STR_HOME_SYSINFO),
             atlas_str(ATLAS_STR_HOME_POWER));
    atlas_ui_footer(hints);
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
