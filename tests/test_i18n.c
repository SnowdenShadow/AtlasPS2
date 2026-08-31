/*
 * AtlasPS2 - tests/test_i18n.c
 *
 * The string tables and the override mechanism.
 *
 * The check that matters most is the completeness one: every key has a
 * non-empty string in both languages. A blank label is a control the
 * user cannot identify, and on a console there is no tooltip to fall
 * back on.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/i18n.h"

static void test_every_key_has_both_languages(void)
{
    int i;

    for (i = 0; i < ATLAS_STR_COUNT; i++) {
        const char *en = atlas_i18n_builtin(ATLAS_LANG_EN,
                                            (atlas_str_id_t)i);
        const char *fr = atlas_i18n_builtin(ATLAS_LANG_FR,
                                            (atlas_str_id_t)i);
        const char *name = atlas_i18n_key_name((atlas_str_id_t)i);

        assert(en && en[0] != '\0');
        assert(fr && fr[0] != '\0');
        assert(name && name[0] != '\0');
    }
}

static void test_key_names_are_unique(void)
{
    /* A duplicate name means one of the two keys can never be
     * overridden by a file, and nothing else would notice. */
    int i, j;

    for (i = 0; i < ATLAS_STR_COUNT; i++) {
        for (j = i + 1; j < ATLAS_STR_COUNT; j++) {
            assert(strcmp(atlas_i18n_key_name((atlas_str_id_t)i),
                          atlas_i18n_key_name((atlas_str_id_t)j)) != 0);
        }
    }
}

static void test_french_is_actually_translated(void)
{
    /*
     * A few keys where an identical string would mean the French was
     * never written. "OK" and "Version" are the same in both and are
     * deliberately not in this list.
     */
    static const atlas_str_id_t ids[] = {
        ATLAS_STR_BACK, ATLAS_STR_SELECT, ATLAS_STR_LAUNCH,
        ATLAS_STR_HOME_WELCOME, ATLAS_STR_HOME_GAMES,
        ATLAS_STR_POWER_OFF, ATLAS_STR_APPS_EMPTY
    };
    int i;

    for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++) {
        assert(strcmp(atlas_i18n_builtin(ATLAS_LANG_EN, ids[i]),
                      atlas_i18n_builtin(ATLAS_LANG_FR, ids[i])) != 0);
    }
}

static void test_language_switching(void)
{
    atlas_i18n_set_lang(ATLAS_LANG_EN);
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Back") == 0);

    atlas_i18n_set_lang(ATLAS_LANG_FR);
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Retour") == 0);
    assert(atlas_i18n_lang() == ATLAS_LANG_FR);

    atlas_i18n_set_lang(ATLAS_LANG_EN);
}

static void test_lang_codes(void)
{
    assert(strcmp(atlas_i18n_lang_code(ATLAS_LANG_EN), "en") == 0);
    assert(strcmp(atlas_i18n_lang_code(ATLAS_LANG_FR), "fr") == 0);

    assert(atlas_i18n_lang_from_code("fr") == ATLAS_LANG_FR);
    assert(atlas_i18n_lang_from_code("FR") == ATLAS_LANG_FR);
    assert(atlas_i18n_lang_from_code("fr_FR") == ATLAS_LANG_FR);
    assert(atlas_i18n_lang_from_code("en") == ATLAS_LANG_EN);

    /* A language we do not have must still boot into a usable UI. */
    assert(atlas_i18n_lang_from_code("de") == ATLAS_LANG_EN);
    assert(atlas_i18n_lang_from_code("") == ATLAS_LANG_EN);
    assert(atlas_i18n_lang_from_code(NULL) == ATLAS_LANG_EN);
}

static void test_overrides(void)
{
    atlas_i18n_set_lang(ATLAS_LANG_EN);
    atlas_i18n_clear_overrides();

    assert(atlas_i18n_set("action.back", "Go back") == 1);
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Go back") == 0);

    /* An unknown key is ignored, not fatal: a file from a later version
     * must not be rejected wholesale by an older build. */
    assert(atlas_i18n_set("action.teleport", "Zap") == 0);
    assert(atlas_i18n_set(NULL, "x") == 0);
    assert(atlas_i18n_set("action.back", NULL) == 0);

    atlas_i18n_clear_overrides();
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Back") == 0);
}

static void test_blank_override_keeps_builtin(void)
{
    /* A key the file left empty falls back rather than blanking the
     * control. */
    atlas_i18n_set_lang(ATLAS_LANG_EN);
    atlas_i18n_clear_overrides();

    assert(atlas_i18n_set("action.back", "") == 0);
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Back") == 0);
}

static void test_overlong_override_is_dropped(void)
{
    /*
     * Dropped, not truncated. The built-in text is complete and in the
     * wrong language at worst; a half-sentence is wrong in every
     * language.
     */
    char big[512];
    int i;

    atlas_i18n_set_lang(ATLAS_LANG_EN);
    atlas_i18n_clear_overrides();

    for (i = 0; i < (int)sizeof(big) - 1; i++)
        big[i] = 'x';
    big[sizeof(big) - 1] = '\0';

    assert(atlas_i18n_set("action.back", big) == 0);
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Back") == 0);
}

/**
 * A few strings are printf formats and the caller passes arguments the
 * BUILT-IN text asks for. An override that changes the conversions -
 * "%s" where the original had "%d", or an extra one that was never
 * pushed - is a crash on a console with nowhere to report it, so it is
 * refused and the built-in text stands.
 */
static void test_override_cannot_change_conversions(void)
{
    atlas_i18n_set_lang(ATLAS_LANG_EN);
    atlas_i18n_clear_overrides();

    /* The type must match: an int read as a pointer is the bad one. */
    assert(atlas_i18n_set("dev.free_kb", "%s KB libres") == 0);

    /* Adding a conversion reads an argument that was never passed. */
    assert(atlas_i18n_set("dev.free_kb", "%d of %d KB free") == 0);

    /* Dropping one is safe for printf but is far more likely a typo
     * than an intention, and it silently loses the number. */
    assert(atlas_i18n_set("dev.free_kb", "some space free") == 0);

    /* Reordering the words around it is exactly what translating is. */
    assert(atlas_i18n_set("dev.free_kb", "libres : %d Ko") == 1);
    assert(strcmp(atlas_str(ATLAS_STR_DEV_FREE_KB), "libres : %d Ko") == 0);

    /* A string with no conversions at all is unaffected. */
    assert(atlas_i18n_set("action.back", "Retour") == 1);

    /* ...including one where the translation introduces a percent sign
     * as text, which printf would read as a conversion. */
    assert(atlas_i18n_set("action.back", "100% back") == 0);
    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Retour") == 0);

    atlas_i18n_clear_overrides();
}

/**
 * Every conversion the English text has, the French text has too, in
 * the same order.
 *
 * The caller writes one snprintf() and it runs in both languages, so a
 * French string carrying "%s" where the English has "%d" is a wrong
 * pointer at a call site that looks correct - and it would only ever
 * happen on a French console. The check runs at build time instead.
 *
 * Deliberately not reusing the module's own comparison: this is
 * checking the table, and a bug shared between the checker and the
 * thing checked would hide exactly the case it exists to catch.
 */
static void test_translations_agree_on_conversions(void)
{
    int i;

    for (i = 0; i < ATLAS_STR_COUNT; i++) {
        const char *en = atlas_i18n_builtin(ATLAS_LANG_EN,
                                            (atlas_str_id_t)i);
        const char *fr = atlas_i18n_builtin(ATLAS_LANG_FR,
                                            (atlas_str_id_t)i);

        for (;;) {
            while (*en && *en != '%')
                en++;
            while (*fr && *fr != '%')
                fr++;

            if (!*en || !*fr) {
                /* One ran out of conversions before the other. */
                assert(!*en && !*fr);
                break;
            }

            /* '%' plus the next character covers "%d", "%s" and "%%".
             * No string in the table uses a width or a flag; if one
             * ever does, this needs to compare the whole run. */
            assert(en[1] == fr[1]);

            en += 2;
            fr += 2;
        }
    }
}

static void test_switching_language_drops_overrides(void)
{
    /* The overrides came from the previous language's file. Keeping
     * them would leave a screen half translated. */
    atlas_i18n_set_lang(ATLAS_LANG_EN);
    atlas_i18n_clear_overrides();

    atlas_i18n_set("action.back", "Go back");
    atlas_i18n_set_lang(ATLAS_LANG_FR);

    assert(strcmp(atlas_str(ATLAS_STR_BACK), "Retour") == 0);

    atlas_i18n_set_lang(ATLAS_LANG_EN);
}

static void test_lookup_never_returns_empty(void)
{
    int i;

    atlas_i18n_clear_overrides();

    for (i = 0; i < ATLAS_STR_COUNT; i++) {
        const char *s = atlas_str((atlas_str_id_t)i);
        assert(s && s[0] != '\0');
    }

    /* Out of range is a visible "?", not a crash and not NULL. */
    assert(strcmp(atlas_str((atlas_str_id_t)-1), "?") == 0);
    assert(strcmp(atlas_str((atlas_str_id_t)ATLAS_STR_COUNT), "?") == 0);
}

static void test_strings_are_valid_utf8(void)
{
    /*
     * The font atlas covers Latin-1, and the accented characters are
     * written as UTF-8 escapes in the table. A byte pair typed wrongly
     * would render as two boxes rather than one letter, which is easy
     * to miss by eye and trivial to check here.
     */
    int lang, i;

    for (lang = 0; lang < ATLAS_LANG_COUNT; lang++) {
        for (i = 0; i < ATLAS_STR_COUNT; i++) {
            const unsigned char *p = (const unsigned char *)
                atlas_i18n_builtin((atlas_lang_t)lang, (atlas_str_id_t)i);

            while (*p) {
                if (*p < 0x80) {
                    p++;
                } else if ((*p & 0xE0) == 0xC0) {
                    assert((p[1] & 0xC0) == 0x80);
                    /* Latin-1 range only, per the font atlas. */
                    assert(*p <= 0xC3);
                    p += 2;
                } else {
                    assert(!"string is not Latin-1 UTF-8");
                }
            }
        }
    }
}

int main(void)
{
    test_every_key_has_both_languages();
    test_key_names_are_unique();
    test_french_is_actually_translated();
    test_language_switching();
    test_lang_codes();
    test_overrides();
    test_blank_override_keeps_builtin();
    test_overlong_override_is_dropped();
    test_override_cannot_change_conversions();
    test_switching_language_drops_overrides();
    test_translations_agree_on_conversions();
    test_lookup_never_returns_empty();
    test_strings_are_valid_utf8();

    printf("test_i18n: all checks passed (%d strings x %d languages)\n",
           ATLAS_STR_COUNT, ATLAS_LANG_COUNT);
    return 0;
}
