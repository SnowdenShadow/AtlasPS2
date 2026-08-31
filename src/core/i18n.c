/*
 * AtlasPS2 - i18n.c
 * The string tables, and the override slots a language file fills in.
 *
 * No file I/O here: this half is pure data and is covered by
 * `make check`. Reading `lang/<code>.ini` off a device lives in
 * i18n_load.c, which is EE-only.
 */
#include <string.h>

#include "atlas/i18n.h"

/* ------------------------------------------------------------------ */
/* Built-in tables                                                     */
/*                                                                     */
/* Three parallel arrays generated from the one X-macro list, so a new */
/* key lands in all three or in none of them.                          */
/* ------------------------------------------------------------------ */

#define X_NAME(id, name, en, fr) name,
#define X_EN(id, name, en, fr)   en,
#define X_FR(id, name, en, fr)   fr,

static const char *const s_names[ATLAS_STR_COUNT] = {
    ATLAS_STR_LIST(X_NAME)
};

static const char *const s_en[ATLAS_STR_COUNT] = {
    ATLAS_STR_LIST(X_EN)
};

static const char *const s_fr[ATLAS_STR_COUNT] = {
    ATLAS_STR_LIST(X_FR)
};

#undef X_NAME
#undef X_EN
#undef X_FR

/* ------------------------------------------------------------------ */
/* Overrides                                                           */
/*                                                                     */
/* A fixed block of storage rather than allocations: this is loaded    */
/* once at boot and freed never, and a launcher that can fragment its  */
/* heap over an evening of use is a launcher that fails to start a     */
/* game at the end of it.                                              */
/*                                                                     */
/* Sized for every key at the length one comfortably fits in. A string */
/* longer than the slot is dropped, not truncated: the built-in text   */
/* is then used, which is complete and in the wrong language at worst. */
/* A half-sentence would be wrong in every language.                   */
/* ------------------------------------------------------------------ */

#define I18N_OVERRIDE_MAX 96

static char s_override[ATLAS_STR_COUNT][I18N_OVERRIDE_MAX];

static atlas_lang_t s_lang = ATLAS_LANG_EN;

/* ------------------------------------------------------------------ */
/* Lookup                                                              */
/* ------------------------------------------------------------------ */

const char *atlas_str(atlas_str_id_t id)
{
    if ((int)id < 0 || (int)id >= ATLAS_STR_COUNT)
        return "?";

    if (s_override[id][0] != '\0')
        return s_override[id];

    return atlas_i18n_builtin(s_lang, id);
}

const char *atlas_i18n_builtin(atlas_lang_t lang, atlas_str_id_t id)
{
    if ((int)id < 0 || (int)id >= ATLAS_STR_COUNT)
        return "?";

    return (lang == ATLAS_LANG_FR) ? s_fr[id] : s_en[id];
}

const char *atlas_i18n_key_name(atlas_str_id_t id)
{
    if ((int)id < 0 || (int)id >= ATLAS_STR_COUNT)
        return "?";

    return s_names[id];
}

/* ------------------------------------------------------------------ */
/* Language                                                            */
/* ------------------------------------------------------------------ */

atlas_lang_t atlas_i18n_lang(void)
{
    return s_lang;
}

void atlas_i18n_set_lang(atlas_lang_t lang)
{
    if ((int)lang < 0 || (int)lang >= ATLAS_LANG_COUNT)
        lang = ATLAS_LANG_EN;

    if (lang == s_lang)
        return;

    /*
     * The overrides came from the previous language's file. Keeping
     * them would leave a screen half translated, which reads as a bug
     * rather than as a missing file.
     */
    atlas_i18n_clear_overrides();
    s_lang = lang;
}

const char *atlas_i18n_lang_code(atlas_lang_t lang)
{
    return (lang == ATLAS_LANG_FR) ? "fr" : "en";
}

atlas_lang_t atlas_i18n_lang_from_code(const char *code)
{
    if (!code)
        return ATLAS_LANG_EN;

    /* Only the first two letters matter, so "fr_FR" and "fr" agree. */
    if ((code[0] == 'f' || code[0] == 'F')
        && (code[1] == 'r' || code[1] == 'R'))
        return ATLAS_LANG_FR;

    return ATLAS_LANG_EN;
}

/* ------------------------------------------------------------------ */
/* Applying a file                                                     */
/* ------------------------------------------------------------------ */

/**
 * Whether an override's conversions match the built-in text's.
 *
 * A few strings are printf formats - "%d KB free", the countdown in the
 * video screen - and the caller passes the arguments the BUILT-IN text
 * asks for. A translation file is a text file on a Memory Card, so a
 * translator who writes "%s" where the original had "%d" would have an
 * int read as a pointer, and a translator who adds a conversion the
 * original does not have would have an argument read that was never
 * pushed. Neither is malice; both are a crash on a console with no
 * console to report it on.
 *
 * The check is deliberately strict rather than clever: the conversions
 * must appear in the same order and be spelled the same way. Comparing
 * only how many there are would accept %d for %s. Translations may
 * still reorder words freely - it is only the % sequences that are
 * pinned, and a string with none of them (which is nearly all of them)
 * is unaffected.
 */
/**
 * Advance `*p` to its next conversion and copy it into `out`.
 *
 * A conversion runs from '%' to the first letter after it, which is the
 * conversion character - or to a second '%', which is a literal percent
 * sign and takes no argument. Everything between is flags, width and
 * precision, and it is copied too: "%5d" and "%d" consume the same
 * argument but a translator swapping one for the other is still a
 * change worth refusing, because it is far more likely to be a typo
 * than an intention.
 *
 * @return non-zero if a conversion was found.
 */
static int is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int next_conversion(const char **p, char *out, int size)
{
    const char *s = *p;
    int n = 0;

    while (*s && *s != '%')
        s++;

    if (!*s) {
        *p = s;
        return 0;
    }

    /* The leading '%'. */
    out[n++] = *s++;

    /* Flags, width and precision, then the conversion character - or a
     * second '%', which ends it immediately as a literal percent. */
    while (*s && n < size - 1) {
        char c = *s++;

        out[n++] = c;

        if (c == '%' || is_letter(c))
            break;
    }

    out[n] = '\0';
    *p = s;

    return 1;
}

static int same_conversions(const char *a, const char *b)
{
    char ca[16], cb[16];

    for (;;) {
        int ga = next_conversion(&a, ca, sizeof(ca));
        int gb = next_conversion(&b, cb, sizeof(cb));

        if (!ga || !gb)
            return ga == gb;

        if (strcmp(ca, cb) != 0)
            return 0;
    }
}

int atlas_i18n_set(const char *name, const char *value)
{
    int i;

    if (!name || !value)
        return 0;

    for (i = 0; i < ATLAS_STR_COUNT; i++) {
        if (strcmp(name, s_names[i]) != 0)
            continue;

        /* Too long to hold whole, or blank: leave the built-in text. */
        if (value[0] == '\0'
            || (int)strlen(value) >= I18N_OVERRIDE_MAX)
            return 0;

        /* Compared against the built-in text for THIS language, which
         * is the string the override replaces and the one whose
         * arguments the caller is passing. */
        if (!same_conversions(atlas_i18n_builtin(s_lang, (atlas_str_id_t)i),
                              value))
            return 0;

        strcpy(s_override[i], value);
        return 1;
    }

    return 0;
}

void atlas_i18n_clear_overrides(void)
{
    memset(s_override, 0, sizeof(s_override));
}
