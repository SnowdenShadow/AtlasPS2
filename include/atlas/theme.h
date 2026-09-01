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

/*
 * The palette, as one row per colour: the struct member and the name
 * that addresses it in a theme.ini.
 *
 * One table rather than two because the field and its file name have to
 * agree, and a struct and a lookup table maintained separately drift
 * the moment a colour is added - silently, into a theme file where the
 * key is simply ignored and the author cannot tell why.
 *
 * The order is also the order the fields are declared in, which is what
 * makes the built-in initialiser below readable.
 */
#define ATLAS_THEME_FIELDS(X)                                             \
    /* Background: a subtle vertical gradient, top to bottom. */          \
    X(bg_top,         "bg_top")                                           \
    X(bg_bottom,      "bg_bottom")                                        \
    /* Panels and separators. */                                          \
    X(panel,          "panel")                                            \
    X(panel_selected, "panel_selected")                                   \
    X(separator,      "separator")                                        \
    /* Text. */                                                           \
    X(text,           "text")            /* primary, white   */           \
    X(text_dim,       "text_dim")        /* secondary, grey  */           \
    X(text_on_accent, "text_on_accent")                                   \
    /* Accents and states. */                                             \
    X(accent,         "accent")          /* electric blue    */           \
    X(accent_dim,     "accent_dim")                                       \
    X(warn,           "warn")                                             \
    X(error,          "error")                                            \
    X(ok,             "ok")                                               \
    /* Header and footer bars. */                                         \
    X(bar,            "bar")                                              \
    X(bar_text,       "bar_text")

typedef struct {
#define ATLAS_THEME_DECL(field, name) u64 field;
    ATLAS_THEME_FIELDS(ATLAS_THEME_DECL)
#undef ATLAS_THEME_DECL
} atlas_theme_t;

/** How many colours a theme has. */
#define ATLAS_THEME_FIELD_COUNT \
    ((int)(sizeof(atlas_theme_t) / sizeof(u64)))

/** The longest theme name, matching ATLAS_CFG_THEME_MAX. */
#define ATLAS_THEME_NAME_MAX 32

/* ------------------------------------------------------------------ */
/* Metrics                                                             */
/*                                                                     */
/* Layout constants live here rather than scattered through the        */
/* screens, so the whole interface can be retuned in one place. These  */
/* are in framebuffer pixels, before the 16:9 x-scale is applied.      */
/* ------------------------------------------------------------------ */

#define ATLAS_UI_HEADER_H    38
#define ATLAS_UI_FOOTER_H    32
#define ATLAS_UI_PAD         20   /* gap between the safe edge and content */
#define ATLAS_UI_ROW_H       36   /* one menu row                          */
#define ATLAS_UI_ROW_GAP     5
#define ATLAS_UI_CORNER      5    /* notch size of the faux-rounded panel  */

/*
 * How far a screen that draws a title pushes its content down, as a
 * multiple of the UI line height.
 *
 * Most screens no longer draw one. A screen whose header bar already
 * names it was spending a whole row restating that name in a larger
 * font, on a field that has ten rows in it - so the header keeps the
 * name and the big title is gone. What remains are the screens whose
 * title says something the bar does not: the wizard, Recovery, the
 * installer, and Home's greeting.
 */
#define ATLAS_UI_TITLE_H     2.0f

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

/** The name of the theme in use, or "" when it is the built-in one. */
const char *atlas_theme_name(void);

/* ------------------------------------------------------------------ */
/* Reading a theme file                                                */
/*                                                                     */
/* Split from the loading so it can be checked on the build machine:   */
/* the colour parser is where a theme file goes wrong, and a check     */
/* that needs a television is not a check.                             */
/* ------------------------------------------------------------------ */

/**
 * Apply one `key = value` pair from a theme.ini to `theme`.
 *
 * The value is a colour: `#RRGGBB`, `#RRGGBBAA`, or the same without
 * the '#'. Alpha is on the GS scale where 0x80 is opaque, so a file
 * asking for 0xFF is clamped rather than over-saturating - a theme
 * author has no way to know that from looking at the number.
 *
 * Unknown keys are ignored, not rejected: a theme written for a later
 * version with more colours in it must still load on this one.
 *
 * @return 1 if the key was recognised and applied, 0 otherwise.
 */
int atlas_theme_set_field(atlas_theme_t *theme, const char *key,
                          const char *value);

/**
 * Parse a whole theme.ini over a copy of the built-in theme.
 *
 * Starting from the built-in one rather than from zero is what makes a
 * partial file safe: a theme naming only an accent colour is a theme
 * with one colour changed, not one with fourteen invisible ones.
 *
 * @param applied  optional; receives how many colours the file set.
 * @return ATLAS_OK, or ATLAS_EINVAL for a bad argument.
 */
atlas_err_t atlas_theme_parse(atlas_theme_t *out, const char *text, int len,
                              int *applied);

/* ------------------------------------------------------------------ */
/* Loading from a device                                               */
/* ------------------------------------------------------------------ */

/** The longest theme.ini this module will read. */
#define ATLAS_THEME_FILE_MAX 4096

/**
 * Load `ATLAS/THEMES/<name>/theme.ini` and make it the active theme.
 *
 * Searched on the Memory Cards before USB, the same order the
 * configuration uses. A name that is empty, or that no device carries,
 * leaves the built-in theme active and reports it - a missing theme is
 * a cosmetic problem and must never be a boot failure.
 *
 * @return ATLAS_OK, ATLAS_ENOENT if no device has that theme,
 *         ATLAS_EINVAL for an empty name.
 */
atlas_err_t atlas_theme_load(const char *name);

/**
 * List the theme names present on the attached devices.
 *
 * Used by the settings screen, which offers what is actually there
 * rather than a text field the user has to type a folder name into.
 *
 * @param names  receives up to `max` names, each ATLAS_THEME_NAME_MAX
 *               bytes; duplicates across devices are listed once.
 * @return how many were written.
 */
int atlas_theme_list(char (*names)[ATLAS_THEME_NAME_MAX], int max);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_THEME_H */
