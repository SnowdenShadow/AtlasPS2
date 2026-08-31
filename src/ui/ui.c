/*
 * AtlasPS2 - ui.c
 * Drawing primitives and shared widgets.
 */
#include <string.h>

#include <gsKit.h>

#include "atlas/ui.h"
#include "atlas/video.h"
#include "atlas/atlas.h"

static atlas_font_t *s_font_ui;
static atlas_font_t *s_font_title;

/*
 * The UI is a single flat layer, so every primitive shares one Z. The
 * depth test is off (see video.c), and gsKit still wants a Z value.
 */
#define UI_Z 1

/* ------------------------------------------------------------------ */
/* Coordinate mapping                                                  */
/*                                                                     */
/* Callers work in safe-area coordinates, where (0,0) is the top-left  */
/* of the visible region rather than of the framebuffer. Mapping here  */
/* means no screen has to remember to add the safe-area origin, and    */
/* the 16:9 narrowing happens in exactly one place.                    */
/* ------------------------------------------------------------------ */

static float map_x(float x)
{
    return (float)atlas_video_safe_x() + x * atlas_video_x_scale();
}

static float map_y(float y)
{
    return (float)atlas_video_safe_y() + y;
}

static float map_w(float w)
{
    return w * atlas_video_x_scale();
}

/* ------------------------------------------------------------------ */
/* Fonts                                                               */
/* ------------------------------------------------------------------ */

void atlas_ui_set_fonts(atlas_font_t *ui, atlas_font_t *title)
{
    s_font_ui = ui;
    s_font_title = title;
}

atlas_font_t *atlas_ui_font(void)       { return s_font_ui; }
atlas_font_t *atlas_ui_font_title(void) { return s_font_title; }

/* ------------------------------------------------------------------ */
/* Shapes                                                              */
/* ------------------------------------------------------------------ */

void atlas_ui_rect(float x, float y, float w, float h, u64 color)
{
    GSGLOBAL *gs = atlas_video_gs();

    if (!gs || w <= 0.0f || h <= 0.0f)
        return;

    gsKit_prim_sprite(gs, map_x(x), map_y(y),
                      map_x(x) + map_w(w), map_y(y) + h,
                      UI_Z, color);
}

void atlas_ui_rect_gradient(float x, float y, float w, float h,
                            u64 top, u64 bottom)
{
    GSGLOBAL *gs = atlas_video_gs();
    float x0, y0, x1, y1;

    if (!gs || w <= 0.0f || h <= 0.0f)
        return;

    x0 = map_x(x);
    y0 = map_y(y);
    x1 = x0 + map_w(w);
    y1 = y0 + h;

    /*
     * A gouraud quad rather than a gouraud sprite: the GS sprite
     * primitive takes a single colour, so a two-colour fill needs the
     * four-vertex form. Corners are listed clockwise from top-left.
     */
    gsKit_prim_quad_gouraud(gs,
                            x0, y0,
                            x1, y0,
                            x0, y1,
                            x1, y1,
                            UI_Z,
                            top, top, bottom, bottom);
}

void atlas_ui_panel(float x, float y, float w, float h, u64 color)
{
    const float c = (float)ATLAS_UI_CORNER;

    if (w <= 0.0f || h <= 0.0f)
        return;

    /*
     * Three stacked rectangles: a full-width middle band and two inset
     * end bands. That notches all four corners with three sprites,
     * which is cheaper than a quad per corner and looks the same at
     * this resolution.
     */
    if (h <= c * 2.0f) {
        atlas_ui_rect(x, y, w, h, color);
        return;
    }

    atlas_ui_rect(x + c, y,             w - c * 2.0f, c,             color);
    atlas_ui_rect(x,     y + c,         w,            h - c * 2.0f,  color);
    atlas_ui_rect(x + c, y + h - c,     w - c * 2.0f, c,             color);
}

void atlas_ui_separator(float x, float y, float w, u64 color)
{
    atlas_ui_rect(x, y, w, 1.0f, color);
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

static float align_x(atlas_font_t *font, float x, atlas_align_t align,
                     const char *text)
{
    float w;

    if (align == ATLAS_ALIGN_LEFT)
        return map_x(x);

    w = atlas_font_width(font, text);

    if (align == ATLAS_ALIGN_RIGHT)
        return map_x(x) - w;

    return map_x(x) - w * 0.5f;
}

void atlas_ui_text(float x, float y, atlas_align_t align, u64 color,
                   const char *text)
{
    if (!s_font_ui || !text)
        return;

    atlas_font_draw(s_font_ui, align_x(s_font_ui, x, align, text),
                    map_y(y), color, text);
}

void atlas_ui_text_title(float x, float y, atlas_align_t align, u64 color,
                         const char *text)
{
    if (!s_font_title || !text)
        return;

    atlas_font_draw(s_font_title, align_x(s_font_title, x, align, text),
                    map_y(y), color, text);
}

void atlas_ui_text_clipped(float x, float y, u64 color, const char *text,
                           float max_w)
{
    if (!s_font_ui || !text)
        return;

    atlas_font_draw_clipped(s_font_ui, map_x(x), map_y(y), color, text,
                            map_w(max_w));
}

float atlas_ui_text_width(const char *text)
{
    if (!s_font_ui || !text)
        return 0.0f;

    /*
     * Returned in safe-area units, not framebuffer pixels, so callers
     * can compare it against the widths they laid out with.
     */
    return atlas_font_width(s_font_ui, text) / atlas_video_x_scale();
}

float atlas_ui_line_height(void)
{
    return (float)atlas_font_line_height(s_font_ui);
}

/* ------------------------------------------------------------------ */
/* Chrome                                                              */
/* ------------------------------------------------------------------ */

void atlas_ui_background(void)
{
    const atlas_theme_t *t = atlas_theme();
    GSGLOBAL *gs = atlas_video_gs();

    if (!gs)
        return;

    /*
     * The background covers the whole framebuffer, not the safe area:
     * the overscan region is still visible on many sets, and leaving it
     * on the clear colour would frame the interface in a hard edge.
     */
    gsKit_prim_quad_gouraud(gs,
                            0.0f, 0.0f,
                            (float)atlas_video_width(), 0.0f,
                            0.0f, (float)atlas_video_height(),
                            (float)atlas_video_width(),
                            (float)atlas_video_height(),
                            UI_Z,
                            t->bg_top, t->bg_top,
                            t->bg_bottom, t->bg_bottom);
}

void atlas_ui_header(const char *right_text)
{
    const atlas_theme_t *t = atlas_theme();
    float w = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float text_y;

    atlas_ui_rect(0.0f, 0.0f, w, (float)ATLAS_UI_HEADER_H, t->bar);
    atlas_ui_separator(0.0f, (float)ATLAS_UI_HEADER_H, w, t->separator);

    /* Centre the label vertically inside the bar. */
    text_y = ((float)ATLAS_UI_HEADER_H - atlas_ui_line_height()) * 0.5f;

    atlas_ui_text(0.0f, text_y, ATLAS_ALIGN_LEFT, t->text, ATLAS_NAME);

    if (right_text)
        atlas_ui_text(w, text_y, ATLAS_ALIGN_RIGHT, t->bar_text, right_text);
}

void atlas_ui_footer(const char *hints)
{
    const atlas_theme_t *t = atlas_theme();
    float w = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float h = (float)atlas_video_safe_h();
    float bar_y = h - (float)ATLAS_UI_FOOTER_H;
    float text_y;

    atlas_ui_separator(0.0f, bar_y, w, t->separator);
    atlas_ui_rect(0.0f, bar_y + 1.0f, w,
                  (float)ATLAS_UI_FOOTER_H - 1.0f, t->bar);

    text_y = bar_y + ((float)ATLAS_UI_FOOTER_H - atlas_ui_line_height()) * 0.5f;

    if (hints)
        atlas_ui_text(0.0f, text_y, ATLAS_ALIGN_LEFT, t->bar_text, hints);
}

/* ------------------------------------------------------------------ */
/* Menu rows                                                           */
/* ------------------------------------------------------------------ */

void atlas_ui_menu_row(float x, float y, float w, int selected,
                       const char *label, const char *value)
{
    const atlas_theme_t *t = atlas_theme();
    float text_y = y + ((float)ATLAS_UI_ROW_H - atlas_ui_line_height()) * 0.5f;
    float text_x = x + (float)ATLAS_UI_PAD;
    float label_max = w - (float)ATLAS_UI_PAD * 2.0f;

    if (selected) {
        atlas_ui_panel(x, y, w, (float)ATLAS_UI_ROW_H, t->panel_selected);

        /*
         * A bar down the left edge rather than a border all round: on an
         * interlaced CRT a one-pixel horizontal line flickers badly,
         * while a vertical one is rock steady.
         */
        atlas_ui_rect(x, y, 3.0f, (float)ATLAS_UI_ROW_H, t->accent);
    } else {
        atlas_ui_panel(x, y, w, (float)ATLAS_UI_ROW_H, t->panel);
    }

    if (value) {
        float vw = atlas_ui_text_width(value);

        atlas_ui_text(x + w - (float)ATLAS_UI_PAD, text_y,
                      ATLAS_ALIGN_RIGHT, t->text_dim, value);

        label_max -= vw + (float)ATLAS_UI_PAD;
    }

    if (label)
        atlas_ui_text_clipped(text_x, text_y,
                              selected ? t->text : t->text_dim,
                              label, label_max);
}

/* ------------------------------------------------------------------ */
/* Message box                                                         */
/* ------------------------------------------------------------------ */

void atlas_ui_message_box(const char *title, const char *body,
                          const char *hint)
{
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float sh = (float)atlas_video_safe_h();
    float lh = atlas_ui_line_height();

    float w = sw * 0.7f;
    float h = lh * 6.0f;
    float x = (sw - w) * 0.5f;
    float y = (sh - h) * 0.5f;
    float ty = y + (float)ATLAS_UI_PAD;

    /*
     * Dim the whole screen behind the box. The GS blends against what is
     * already in the framebuffer, so a half-alpha black sprite over the
     * finished frame darkens it without needing a second pass.
     */
    atlas_ui_rect(0.0f, 0.0f, sw, sh, ATLAS_RGBA(0x00, 0x00, 0x00, 0x50));

    atlas_ui_panel(x, y, w, h, t->panel);
    atlas_ui_rect(x, y, w, 3.0f, t->accent);

    if (title) {
        atlas_ui_text(x + (float)ATLAS_UI_PAD, ty, ATLAS_ALIGN_LEFT,
                      t->text, title);
        ty += lh * 1.5f;
    }

    if (body)
        atlas_ui_text_clipped(x + (float)ATLAS_UI_PAD, ty, t->text_dim,
                              body, w - (float)ATLAS_UI_PAD * 2.0f);

    if (hint)
        atlas_ui_text(x + w * 0.5f, y + h - lh - (float)ATLAS_UI_PAD,
                      ATLAS_ALIGN_CENTER, t->text_dim, hint);
}
