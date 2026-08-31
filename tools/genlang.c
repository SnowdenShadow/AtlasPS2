/*
 * AtlasPS2 - tools/genlang.c
 * Writes lang/en.ini and lang/fr.ini from the built-in tables.
 *
 * WHY THIS IS GENERATED
 * ---------------------
 * The shipped translation files are complete copies of what is compiled
 * in, so a user can open one and see every string with its key rather
 * than having to guess the names. Two hand-maintained copies of the
 * same list drift within a release; a generator cannot.
 *
 * Runs on the build machine with the host compiler - it only needs the
 * string tables, which carry no PS2 dependency.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/i18n.h"

static int write_lang(const char *dir, atlas_lang_t lang)
{
    char path[512];
    FILE *f;
    int i;

    snprintf(path, sizeof(path), "%s/%s.ini", dir,
             atlas_i18n_lang_code(lang));

    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "genlang: cannot write %s\n", path);
        return 1;
    }

    fprintf(f,
        "# " ATLAS_NAME " " ATLAS_VERSION_STRING " - %s translation\n"
        "#\n"
        "# Generated from the built-in tables by `make lang`. Edit it to\n"
        "# change what appears on screen: put the file at\n"
        "#     mc0:/ATLAS/LANG/%s.ini\n"
        "# and it is read at start-up.\n"
        "#\n"
        "# A key you delete, leave blank, or misspell falls back to the\n"
        "# built-in text, so a partly translated file is safe. UTF-8,\n"
        "# and accented characters are fine.\n"
        "\n",
        lang == ATLAS_LANG_FR ? "French" : "English",
        atlas_i18n_lang_code(lang));

    for (i = 0; i < ATLAS_STR_COUNT; i++)
        fprintf(f, "%s=%s\n", atlas_i18n_key_name((atlas_str_id_t)i),
                atlas_i18n_builtin(lang, (atlas_str_id_t)i));

    fclose(f);

    printf("genlang: %s (%d strings)\n", path, ATLAS_STR_COUNT);
    return 0;
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : "lang";

    if (write_lang(dir, ATLAS_LANG_EN) != 0)
        return 1;

    if (write_lang(dir, ATLAS_LANG_FR) != 0)
        return 1;

    return 0;
}
