/*
 * AtlasPS2 - font.c
 * Bitmap font upload and text drawing.
 */
#include <stdlib.h>
#include <string.h>

#include <gsKit.h>
#include <malloc.h>

#include "atlas/font.h"
#include "atlas/video.h"
#include "atlas/log.h"

struct atlas_font {
    const atlas_font_data_t *data;
    GSTEXTURE tex;
    u32 *clut;     /* 256-entry greyscale palette, owned */
    u32 *pixels;   /* aligned copy of the atlas, owned   */
    int uploaded;
};

/*
 * The GS treats alpha 0x80 as fully opaque, not 0xFF - values above 0x80
 * over-saturate. Every colour AtlasPS2 builds uses this constant.
 */
#define ATLAS_ALPHA_OPAQUE 0x80

/* ------------------------------------------------------------------ */
/* Creation                                                            */
/* ------------------------------------------------------------------ */

/**
 * Build the CLUT for an 8-bit alpha atlas.
 *
 * Entry i is white with alpha proportional to i, so drawing the texture
 * modulated by a primitive colour tints the glyphs to that colour and
 * uses the atlas coverage as the alpha mask. One atlas, any colour.
 *
 * The GS stores a 256-entry CT32 CLUT in a swizzled order: within each
 * group of 32 entries, the middle two blocks of 8 are swapped. Writing
 * the palette linearly produces visibly wrong colours for indices 8..23
 * of every block, so we undo that here.
 */
static void build_clut(u32 *clut)
{
    int i;

    for (i = 0; i < 256; i++) {
        int alpha = (i * ATLAS_ALPHA_OPAQUE) / 255;
        u32 entry = 0x00FFFFFFu | ((u32)alpha << 24);

        /* CSM1 swizzle: swap the two middle octets of each 32-entry block. */
        int block = i & ~31;
        int idx = i & 31;
        int swizzled;

        if (idx >= 8 && idx < 16)
            swizzled = block + idx + 8;
        else if (idx >= 16 && idx < 24)
            swizzled = block + idx - 8;
        else
            swizzled = i;

        clut[swizzled] = entry;
    }
}

atlas_font_t *atlas_font_create(const atlas_font_data_t *data)
{
    atlas_font_t *font;
    GSGLOBAL *gs = atlas_video_gs();
    u32 pixel_bytes;
    u32 vram;

    if (!data || !gs)
        return NULL;

    font = (atlas_font_t *)calloc(1, sizeof(*font));
    if (!font)
        return NULL;

    font->data = data;

    /*
     * gsKit DMAs from these buffers, so both need 16-byte alignment that
     * a plain static const array in the generated file cannot guarantee
     * once the linker has had its way. Copying once at startup is
     * cheaper than fighting the linker.
     */
    pixel_bytes = (u32)(data->atlas_w * data->atlas_h);

    font->pixels = (u32 *)memalign(128, (pixel_bytes + 127) & ~127u);
    font->clut   = (u32 *)memalign(128, 256 * sizeof(u32));

    if (!font->pixels || !font->clut) {
        atlas_font_destroy(font);
        return NULL;
    }

    memcpy(font->pixels, data->pixels, pixel_bytes);
    build_clut(font->clut);

    font->tex.Width  = data->atlas_w;
    font->tex.Height = data->atlas_h;
    font->tex.PSM    = GS_PSM_T8;
    font->tex.ClutPSM = GS_PSM_CT32;
    font->tex.Mem    = font->pixels;
    font->tex.Clut   = font->clut;
    font->tex.Filter = GS_FILTER_NEAREST; /* crisp text; LINEAR would
                                           * blur 12 px glyphs to mush */
    font->tex.ClutStorageMode = GS_CLUT_STORAGE_CSM1;
    font->tex.Delayed = 0;

    /* gsKit computes TBW itself in gsKit_texture_upload. */
    font->tex.TBW = 0;

    vram = gsKit_vram_alloc(gs,
                            gsKit_texture_size(font->tex.Width,
                                               font->tex.Height,
                                               font->tex.PSM),
                            GSKIT_ALLOC_USERBUFFER);
    if (vram == GSKIT_ALLOC_ERROR) {
        ATLAS_LOG("FONT", "no VRAM for %dx%d atlas",
                  font->tex.Width, font->tex.Height);
        atlas_font_destroy(font);
        return NULL;
    }
    font->tex.Vram = vram;

    vram = gsKit_vram_alloc(gs,
                            gsKit_texture_size(16, 16, GS_PSM_CT32),
                            GSKIT_ALLOC_USERBUFFER);
    if (vram == GSKIT_ALLOC_ERROR) {
        ATLAS_LOG("FONT", "no VRAM for CLUT");
        atlas_font_destroy(font);
        return NULL;
    }
    font->tex.VramClut = vram;

    gsKit_texture_upload(gs, &font->tex);
    font->uploaded = 1;

    ATLAS_LOG("FONT", "uploaded %dx%d atlas, line height %d",
              data->atlas_w, data->atlas_h, data->line_height);

    return font;
}

void atlas_font_destroy(atlas_font_t *font)
{
    if (!font)
        return;

    /*
     * The VRAM is not freed individually: gsKit's allocator is a bump
     * pointer with no per-allocation free. Fonts live for the whole
     * session, and a video mode change calls gsKit_vram_clear() which
     * resets the whole pool, so this is not a leak in practice.
     */
    free(font->pixels);
    free(font->clut);
    free(font);
}

/* ------------------------------------------------------------------ */
/* Glyph lookup                                                        */
/* ------------------------------------------------------------------ */

static const atlas_glyph_t *glyph_for(const atlas_font_data_t *data, int code)
{
    if (code < data->first_char || code > data->last_char)
        code = '?';

    if (code < data->first_char || code > data->last_char)
        return NULL;

    return &data->glyphs[code - data->first_char];
}

/**
 * Decode one UTF-8 code point, advancing *p.
 *
 * The translation files are UTF-8 so French accents arrive as two-byte
 * sequences; our atlas covers Latin-1, so anything above U+00FF becomes
 * '?'. Malformed bytes advance by one to guarantee progress - a decoder
 * that can stall would hang the render loop on a corrupt string.
 */
static int utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    int cp;

    if (s[0] < 0x80) {
        cp = s[0];
        *p += 1;
    } else if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2;
    } else if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80
               && (s[2] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3;
    } else {
        cp = '?';
        *p += 1;
    }

    return (cp > 0xFF) ? '?' : cp;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

void atlas_font_draw_scaled(atlas_font_t *font, float x, float y, float scale,
                            u64 color, const char *text)
{
    GSGLOBAL *gs = atlas_video_gs();
    const char *p = text;
    float pen = x;

    if (!font || !gs || !text || !font->uploaded)
        return;

    while (*p) {
        int cp = utf8_next(&p);
        const atlas_glyph_t *g = glyph_for(font->data, cp);

        if (!g)
            continue;

        if (g->w > 0 && g->h > 0) {
            float gx = pen + (float)g->bearing_x * scale;
            float gy = y + (float)g->bearing_y * scale;

            gsKit_prim_sprite_texture(gs, &font->tex,
                                      gx, gy,
                                      (float)g->x, (float)g->y,
                                      gx + (float)g->w * scale,
                                      gy + (float)g->h * scale,
                                      (float)(g->x + g->w),
                                      (float)(g->y + g->h),
                                      1, color);
        }

        pen += (float)g->advance * scale;
    }
}

void atlas_font_draw(atlas_font_t *font, float x, float y, u64 color,
                     const char *text)
{
    atlas_font_draw_scaled(font, x, y, 1.0f, color, text);
}

void atlas_font_draw_clipped(atlas_font_t *font, float x, float y, u64 color,
                             const char *text, float max_width)
{
    char buf[128];
    float ellipsis_w;
    size_t len;

    if (!font || !text)
        return;

    if (atlas_font_width(font, text) <= max_width) {
        atlas_font_draw(font, x, y, color, text);
        return;
    }

    ellipsis_w = atlas_font_width(font, "...");

    /*
     * Trim one byte at a time from a working copy until the text plus
     * the ellipsis fits. Byte-wise trimming can cut a UTF-8 sequence in
     * half, so we then back up over any continuation bytes; the decoder
     * would otherwise render a stray '?'.
     */
    len = strlen(text);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;

    memcpy(buf, text, len);
    buf[len] = '\0';

    while (len > 0) {
        buf[len] = '\0';

        if (atlas_font_width(font, buf) + ellipsis_w <= max_width)
            break;

        len--;

        /* Do not leave a dangling UTF-8 continuation byte. */
        while (len > 0 && ((unsigned char)buf[len] & 0xC0) == 0x80)
            len--;
    }

    atlas_font_draw(font, x, y, color, buf);
    atlas_font_draw(font, x + atlas_font_width(font, buf), y, color, "...");
}

/* ------------------------------------------------------------------ */
/* Measurement                                                         */
/* ------------------------------------------------------------------ */

float atlas_font_width_scaled(atlas_font_t *font, const char *text, float scale)
{
    const char *p = text;
    float w = 0.0f;

    if (!font || !text)
        return 0.0f;

    while (*p) {
        int cp = utf8_next(&p);
        const atlas_glyph_t *g = glyph_for(font->data, cp);

        if (g)
            w += (float)g->advance * scale;
    }

    return w;
}

float atlas_font_width(atlas_font_t *font, const char *text)
{
    return atlas_font_width_scaled(font, text, 1.0f);
}

int atlas_font_line_height(atlas_font_t *font)
{
    return font ? font->data->line_height : 0;
}
