/*
 * AtlasPS2 - theme.c
 * The built-in palette and the active-theme slot.
 */
#include <string.h>

#include "atlas/theme.h"

/*
 * The default AtlasPS2 look: near-black blue ground, electric blue
 * accent, white on grey text. Tuned for a CRT, where thin light-on-dark
 * text blooms and saturated reds bleed - hence the desaturated error
 * colour and the wide gap between primary and secondary text.
 */
static const atlas_theme_t s_builtin = {
    ATLAS_RGB(0x0C, 0x0F, 0x16),  /* bg_top         */
    ATLAS_RGB(0x14, 0x18, 0x24),  /* bg_bottom      */

    ATLAS_RGB(0x1A, 0x1F, 0x2B),  /* panel          */
    ATLAS_RGB(0x24, 0x33, 0x4D),  /* panel_selected */
    ATLAS_RGB(0x2A, 0x31, 0x3F),  /* separator      */

    ATLAS_RGB(0xEC, 0xEF, 0xF4),  /* text           */
    ATLAS_RGB(0x8B, 0x95, 0xA6),  /* text_dim       */
    ATLAS_RGB(0x08, 0x0C, 0x14),  /* text_on_accent */

    ATLAS_RGB(0x3D, 0x9B, 0xFF),  /* accent         */
    ATLAS_RGB(0x22, 0x55, 0x8C),  /* accent_dim     */
    ATLAS_RGB(0xFF, 0xB4, 0x4A),  /* warn           */
    ATLAS_RGB(0xE8, 0x6B, 0x6B),  /* error          */
    ATLAS_RGB(0x5F, 0xC9, 0x8A),  /* ok             */

    ATLAS_RGB(0x10, 0x14, 0x1E),  /* bar            */
    ATLAS_RGB(0x8B, 0x95, 0xA6)   /* bar_text       */
};

/*
 * A loaded theme is copied here rather than referenced, so the caller
 * can free the file buffer it parsed and a half-freed theme can never
 * become the live palette.
 */
static atlas_theme_t s_active = {0};
static int s_custom;

const atlas_theme_t *atlas_theme_builtin(void)
{
    return &s_builtin;
}

const atlas_theme_t *atlas_theme(void)
{
    return s_custom ? &s_active : &s_builtin;
}

void atlas_theme_set(const atlas_theme_t *theme)
{
    if (!theme) {
        s_custom = 0;
        return;
    }

    memcpy(&s_active, theme, sizeof(s_active));
    s_custom = 1;
}
