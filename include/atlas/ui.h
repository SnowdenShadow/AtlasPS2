/*
 * AtlasPS2 - ui.h
 *
 * Drawing primitives and widgets shared by every screen.
 *
 * Screens never call gsKit directly: they compose from these, so the
 * whole interface picks up a theme change or a layout fix in one place.
 * Everything here takes coordinates in the SAFE AREA, and applies the
 * 16:9 x-scale itself, so a layout authored once looks right in both
 * aspects on both PAL and NTSC.
 */
#ifndef ATLAS_UI_H
#define ATLAS_UI_H

#include "atlas/atlas.h"
#include "atlas/font.h"
#include "atlas/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Fonts                                                               */
/* ------------------------------------------------------------------ */

/**
 * Hand the UI the fonts it should draw with.
 *
 * Both may be NULL, in which case the text widgets draw nothing rather
 * than crashing - a font that failed to upload must not take the whole
 * interface down with it.
 */
void atlas_ui_set_fonts(atlas_font_t *ui, atlas_font_t *title);

atlas_font_t *atlas_ui_font(void);
atlas_font_t *atlas_ui_font_title(void);

/* ------------------------------------------------------------------ */
/* Shapes                                                              */
/* ------------------------------------------------------------------ */

/** Fill a rectangle. */
void atlas_ui_rect(float x, float y, float w, float h, u64 color);

/** Fill a rectangle with a vertical gradient. */
void atlas_ui_rect_gradient(float x, float y, float w, float h,
                            u64 top, u64 bottom);

/**
 * Fill a rectangle with its corners notched off.
 *
 * The GS has no rounded-rectangle primitive and no cheap anti-aliasing,
 * so a "rounded" panel is a rectangle with small triangles cut from the
 * corners. At 640x448 on a CRT this reads as rounded.
 */
void atlas_ui_panel(float x, float y, float w, float h, u64 color);

/** A one-pixel horizontal rule. */
void atlas_ui_separator(float x, float y, float w, u64 color);

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

typedef enum {
    ATLAS_ALIGN_LEFT = 0,
    ATLAS_ALIGN_CENTER,
    ATLAS_ALIGN_RIGHT
} atlas_align_t;

/** Draw UI-font text. x is the left, centre or right edge per `align`. */
void atlas_ui_text(float x, float y, atlas_align_t align, u64 color,
                   const char *text);

/** Draw title-font text. */
void atlas_ui_text_title(float x, float y, atlas_align_t align, u64 color,
                         const char *text);

/** Draw UI text clipped to `max_w`, with an ellipsis if it overflows. */
void atlas_ui_text_clipped(float x, float y, u64 color, const char *text,
                           float max_w);

/** Width of `text` in the UI font, in safe-area units. */
float atlas_ui_text_width(const char *text);

/** Height of one UI-font line. */
float atlas_ui_line_height(void);

/* ------------------------------------------------------------------ */
/* Chrome                                                              */
/* ------------------------------------------------------------------ */

/**
 * The header bar: the AtlasPS2 mark on the left, `right_text` on the
 * right (device indicators, a clock, a screen name). `right_text` may be
 * NULL.
 */
void atlas_ui_header(const char *right_text);

/**
 * The footer bar: button hints, e.g. "X Select   O Back".
 *
 * Pass the logical meaning, not the button glyph - this function draws
 * the physical button that currently means confirm, so the hint stays
 * correct on a Japanese-layout console.
 */
void atlas_ui_footer(const char *hints);

/** Fill the whole screen with the theme's background gradient. */
void atlas_ui_background(void);

/* ------------------------------------------------------------------ */
/* Menu rows                                                           */
/* ------------------------------------------------------------------ */

/**
 * One row of a vertical menu.
 *
 * @param index    row position, 0-based; the row's y is derived from it
 * @param selected draw as the highlighted row
 * @param label    left-aligned primary text
 * @param value    right-aligned secondary text, or NULL
 */
void atlas_ui_menu_row(float x, float y, float w, int selected,
                       const char *label, const char *value);

/**
 * A modal message box with a title and body, centred on screen.
 * Draws only - the caller owns the input loop and decides what dismisses
 * it, because a confirmation and an error need different answers.
 */
void atlas_ui_message_box(const char *title, const char *body,
                          const char *hint);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_UI_H */
