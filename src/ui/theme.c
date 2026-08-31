/*
 * AtlasPS2 - theme.c
 * The built-in palette, the active-theme slot, and the colour parser.
 *
 * No device access and no gsKit: this file is compiled by `make check`
 * on the build machine, because a theme file's colours are exactly the
 * thing that is wrong when a theme looks wrong, and a check that needs
 * a television is not a check. The half that reads files from a Memory
 * Card is theme_io.c.
 */
#include <string.h>

#include "atlas/theme.h"

/*
 * The default AtlasPS2 look: near-black blue ground, electric blue
 * accent, white on grey text. Tuned for a CRT, where thin light-on-dark
 * text blooms and saturated reds bleed - hence the desaturated error
 * colour and the wide gap between primary and secondary text.
 *
 * Field order follows ATLAS_THEME_FIELDS, so this list and the struct
 * cannot fall out of step.
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
static int  s_custom;
static char s_name[ATLAS_THEME_NAME_MAX];

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
        s_name[0] = '\0';
        return;
    }

    memcpy(&s_active, theme, sizeof(s_active));
    s_custom = 1;
}

const char *atlas_theme_name(void)
{
    return s_custom ? s_name : "";
}

/** Internal: theme_io.c records which theme it installed. */
void atlas_theme_set_name_(const char *name)
{
    int i;

    if (!name) {
        s_name[0] = '\0';
        return;
    }

    for (i = 0; i < ATLAS_THEME_NAME_MAX - 1 && name[i]; i++)
        s_name[i] = name[i];

    s_name[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* Colours                                                             */
/* ------------------------------------------------------------------ */

/** Hex digit value, or -1. */
static int hex_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

/**
 * Parse "#RRGGBB" or "#RRGGBBAA" (the '#' optional) into a GS colour.
 *
 * Rejected rather than best-guessed: a value with a stray character in
 * it is a typo, and a typo that half-parses gives a colour the author
 * did not choose and cannot account for. Leaving the built-in colour in
 * place is the outcome they can actually diagnose.
 *
 * @return 1 on success.
 */
static int parse_color(const char *s, u64 *out)
{
    int v[8];
    int n = 0;
    int r, g, b, a;

    if (!s || !out)
        return 0;

    if (*s == '#')
        s++;

    while (*s && n < 8) {
        int h = hex_val(*s);

        if (h < 0)
            return 0;

        v[n++] = h;
        s++;
    }

    /* Trailing junk, or not one of the two accepted lengths. */
    if (*s != '\0' || (n != 6 && n != 8))
        return 0;

    r = v[0] * 16 + v[1];
    g = v[2] * 16 + v[3];
    b = v[4] * 16 + v[5];

    /*
     * Opaque unless the file says otherwise. On the GS 0x80 is 1.0 and
     * anything above it over-saturates instead of getting brighter, so
     * a file asking for the 0xFF that every other system calls "opaque"
     * is clamped rather than obeyed. The author has no way to know that
     * from the number, and a blown-out panel looks like a bug in
     * AtlasPS2 rather than a value out of range.
     */
    a = (n == 8) ? v[6] * 16 + v[7] : ATLAS_ALPHA_OPAQUE;

    if (a > ATLAS_ALPHA_OPAQUE)
        a = ATLAS_ALPHA_OPAQUE;

    *out = ATLAS_RGBA((u64)r, (u64)g, (u64)b, (u64)a);

    return 1;
}

int atlas_theme_set_field(atlas_theme_t *theme, const char *key,
                          const char *value)
{
    u64 color;

    if (!theme || !key || !value)
        return 0;

    if (!parse_color(value, &color))
        return 0;

#define ATLAS_THEME_MATCH(field, name)      \
    if (strcmp(key, name) == 0) {           \
        theme->field = color;               \
        return 1;                           \
    }

    ATLAS_THEME_FIELDS(ATLAS_THEME_MATCH)
#undef ATLAS_THEME_MATCH

    return 0;
}

/* ------------------------------------------------------------------ */
/* Parsing a file                                                      */
/*                                                                     */
/* The INI reader is not used here. It calls back per key and would    */
/* pull ini.c into every program that draws; more to the point, a      */
/* theme file is fourteen `name = #RRGGBB` lines and reading it wants  */
/* no section handling at all. What it does share is the tolerance:    */
/* CRLF, comments, spacing, and one bad line costing one colour.       */
/* ------------------------------------------------------------------ */

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

atlas_err_t atlas_theme_parse(atlas_theme_t *out, const char *text, int len,
                              int *applied)
{
    int i = 0;
    int count = 0;

    if (!out || !text || len < 0)
        return ATLAS_EINVAL;

    /*
     * Every field starts at the built-in value, which is what makes a
     * partial file safe: a theme naming only an accent colour is a
     * theme with one colour changed, not one with fourteen invisible
     * ones.
     */
    *out = s_builtin;

    while (i < len) {
        char key[ATLAS_THEME_NAME_MAX];
        char val[ATLAS_THEME_NAME_MAX];
        int start, end, k = 0, v = 0;

        /* One line. */
        start = i;
        while (i < len && text[i] != '\n')
            i++;
        end = i;
        if (i < len)
            i++; /* step over the newline */

        while (start < end && is_space(text[start]))
            start++;

        /* Blank, a comment, or a section header we have no use for:
         * a theme file has one section at most and its name is the
         * folder it sits in. */
        if (start >= end || text[start] == '#' || text[start] == ';'
            || text[start] == '[')
            continue;

        /* key */
        while (start < end && text[start] != '='
               && k < (int)sizeof(key) - 1) {
            key[k++] = text[start++];
        }

        /* No '=' on the line, or a key too long to be one of ours. */
        if (start >= end || text[start] != '=')
            continue;

        start++; /* the '=' */

        while (k > 0 && is_space(key[k - 1]))
            k--;
        key[k] = '\0';

        /* value */
        while (start < end && is_space(text[start]))
            start++;

        while (start < end && v < (int)sizeof(val) - 1) {
            char c = text[start];

            /* A trailing comment is not part of a colour. */
            if (c == '#' && v > 0)
                break;

            val[v++] = c;
            start++;
        }

        while (v > 0 && is_space(val[v - 1]))
            v--;
        val[v] = '\0';

        if (atlas_theme_set_field(out, key, val))
            count++;
    }

    if (applied)
        *applied = count;

    return ATLAS_OK;
}
