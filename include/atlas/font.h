/*
 * AtlasPS2 - font.h
 *
 * Bitmap font rendering.
 *
 * The atlas itself is baked at build time by tools/genfont.py from a
 * redistributable open font; see that script for why we do not use
 * gsKit's FONTM helper (it reads Sony ROM data).
 *
 * The atlas is an 8-bit alpha texture (GS_PSM_T8) with a greyscale CLUT,
 * so text can be tinted any colour at draw time by modulating the
 * primitive colour, and one atlas serves every colour the UI uses.
 */
#ifndef ATLAS_FONT_H
#define ATLAS_FONT_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One glyph's placement in the atlas and its metrics. */
typedef struct {
    short x, y;      /* top-left in the atlas, pixels          */
    short w, h;      /* size in the atlas, pixels              */
    short bearing_x; /* horizontal offset from the pen position */
    short bearing_y; /* vertical offset from the line top       */
    short advance;   /* how far to move the pen after drawing   */
} atlas_glyph_t;

/** A baked font. Generated files define one of these. */
typedef struct {
    const unsigned char *pixels; /* atlas_w * atlas_h alpha bytes */
    int atlas_w;
    int atlas_h;
    const atlas_glyph_t *glyphs; /* last_char - first_char + 1 entries */
    int first_char;
    int last_char;
    int ascent;
    int line_height;
} atlas_font_data_t;

/** A font that has been uploaded to GS VRAM and is ready to draw. */
typedef struct atlas_font atlas_font_t;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * Upload a baked font to VRAM.
 *
 * Requires atlas_video_init() to have succeeded.
 *
 * @return the font handle, or NULL if VRAM could not be allocated.
 */
atlas_font_t *atlas_font_create(const atlas_font_data_t *data);

/** Release a font and its VRAM. */
void atlas_font_destroy(atlas_font_t *font);

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/**
 * Draw a UTF-8 string. Characters outside Latin-1 are drawn as '?'
 * rather than dropped, so a missing glyph is visible instead of silently
 * shortening the text.
 *
 * @param x,y   pen position; y is the TOP of the line, not the baseline,
 *              because laying out a menu against line tops is simpler.
 * @param color GS_SETREG_RGBAQ colour. Alpha 0x80 is fully opaque on the
 *              PS2 (the GS uses 0x80, not 0xFF, as "1.0").
 */
void atlas_font_draw(atlas_font_t *font, float x, float y, u64 color,
                     const char *text);

/** Like atlas_font_draw() but scales the glyphs by `scale`. */
void atlas_font_draw_scaled(atlas_font_t *font, float x, float y, float scale,
                            u64 color, const char *text);

/**
 * Draw text clipped to `max_width` pixels, appending an ellipsis when it
 * does not fit. Used for application names, which come from user folders
 * and can be arbitrarily long.
 */
void atlas_font_draw_clipped(atlas_font_t *font, float x, float y, u64 color,
                             const char *text, float max_width);

/* ------------------------------------------------------------------ */
/* Measurement                                                         */
/* ------------------------------------------------------------------ */

/** Width the string would occupy, in pixels. */
float atlas_font_width(atlas_font_t *font, const char *text);

float atlas_font_width_scaled(atlas_font_t *font, const char *text, float scale);

/** Distance between consecutive line tops, in pixels. */
int atlas_font_line_height(atlas_font_t *font);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_FONT_H */
