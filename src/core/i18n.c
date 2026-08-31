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

        strcpy(s_override[i], value);
        return 1;
    }

    return 0;
}

void atlas_i18n_clear_overrides(void)
{
    memset(s_override, 0, sizeof(s_override));
}
