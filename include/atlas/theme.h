/*
 * AtlasPS2 - theme.h
 *
 * The interface palette and metrics.
 *
 * WHY THERE IS ALWAYS A BUILT-IN THEME
 * ------------------------------------
 * The default theme is compiled in and cannot be removed or edited. A
 * theme loaded from a device only overrides fields on top of it, so a
 * theme file that is truncated, corrupt or simply missing degrades to a
 * readable interface instead of an unreadable one - and Recovery mode
 * ignores loaded themes entirely.
 *
 * Colours are GS_SETREG_RGBAQ values, ready to hand to a primitive.
 * Alpha 0x80 is fully opaque on the GS; higher values over-saturate.
 */
#ifndef ATLAS_THEME_H
#define ATLAS_THEME_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Build a theme colour. Alpha defaults to opaque. */
#define ATLAS_RGB(r, g, b)      ATLAS_RGBA((r), (g), (b), 0x80)
#define ATLAS_RGBA(r, g, b, a) \
    ((u64)(r) | ((u64)(g) << 8) | ((u64)(b) << 16) | ((u64)(a) << 24))

/** Fully opaque on the GS. Not 0xFF - see the header comment. */
#define ATLAS_ALPHA_OPAQUE 0x80

typedef struct {
    /* Background: a subtle vertical gradient, top to bottom. */
    u64 bg_top;
    u64 bg_bottom;

    /* Panels and separators. */
    u64 panel;
    u64 panel_selected;
    u64 separator;

    /* Text. */
    u64 text;        /* primary, white                */
    u64 text_dim;    /* secondary, grey               */
    u64 text_on_accent;

    /* Accents and states. */
    u64 accent;      /* electric blue                 */
    u64 accent_dim;
    u64 warn;
    u64 error;
    u64 ok;

    /* Header and footer bars. */
    u64 bar;
    u64 bar_text;
} atlas_theme_t;

/* ------------------------------------------------------------------ */
/* Metrics                                                             */
/*                                                                     */
/* Layout constants live here rather than scattered through the        */
/* screens, so the whole interface can be retuned in one place. These  */
/* are in framebuffer pixels, before the 16:9 x-scale is applied.      */
/* ------------------------------------------------------------------ */

#define ATLAS_UI_HEADER_H    34
#define ATLAS_UI_FOOTER_H    28
#define ATLAS_UI_PAD         16   /* gap between the safe edge and content */
#define ATLAS_UI_ROW_H       32   /* one menu row                          */
#define ATLAS_UI_ROW_GAP     4
#define ATLAS_UI_CORNER      4    /* notch size of the faux-rounded panel  */

/** The compiled-in theme. Never NULL, never changes. */
const atlas_theme_t *atlas_theme_builtin(void);

/** The theme in use. Falls back to the built-in one. Never NULL. */
const atlas_theme_t *atlas_theme(void);

/**
 * Install a theme, or NULL to return to the built-in one.
 *
 * The pointer is copied, so the caller may free its own storage after
 * this returns - a theme loaded from a device should not have to stay
 * resident just because it is active.
 */
void atlas_theme_set(const atlas_theme_t *theme);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_THEME_H */
