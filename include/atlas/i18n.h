/*
 * AtlasPS2 - i18n.h
 *
 * Every string the user sees.
 *
 * WHY THE TABLES ARE BUILT IN
 * ---------------------------
 * The spec asks for translation files rather than hardcoded text, and
 * this module loads them - but it also carries a complete built-in copy
 * of both languages, because Recovery mode has to draw a readable
 * screen when nothing is mounted and the Memory Card is exactly what
 * failed. A launcher whose error messages live on the device that broke
 * is a launcher that shows a blank screen at the one moment it matters.
 *
 * So a file overrides, it never supplies. `lang/en.ini` and
 * `lang/fr.ini` ship as complete, editable copies of the built-in
 * tables - generated from them by `make lang`, so the two cannot drift.
 *
 * WHY KEYS ARE AN ENUM
 * --------------------
 * Lookup is an array index, not a hash of a string, so drawing a screen
 * costs no string comparisons. More usefully, the X-macro list below
 * carries the key, its file name and BOTH translations on one row: a
 * key cannot be added without a French string, because there is nowhere
 * to put it that the compiler will accept.
 */
#ifndef ATLAS_I18N_H
#define ATLAS_I18N_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* The strings                                                         */
/*                                                                     */
/* X(id, name-in-file, english, french)                                */
/*                                                                     */
/* French is the reference translation, per the spec, so it is written  */
/* first and English follows it rather than the other way round.        */
/* Accented characters are fine: the font atlas covers Latin-1 and the  */
/* UTF-8 decoder is checked against every French accent.               */
/* ------------------------------------------------------------------ */

#define ATLAS_STR_LIST(X)                                                    \
    /* Generic actions, used in footers across screens. */                   \
    X(BACK,            "action.back",       "Back",           "Retour")      \
    X(SELECT,          "action.select",     "Select",         "Choisir")     \
    X(LAUNCH,          "action.launch",     "Launch",         "Lancer")      \
    X(RESCAN,          "action.rescan",     "Rescan",         "Actualiser")  \
    X(CANCEL,          "action.cancel",     "Cancel",         "Annuler")     \
    X(CONFIRM,         "action.confirm",    "Confirm",        "Confirmer")   \
    X(OK,              "action.ok",         "OK",             "OK")          \
    /* Home. */                                                              \
    X(HOME_WELCOME,    "home.welcome",      "Welcome",        "Bienvenue")   \
    X(HOME_GAMES,      "home.games",        "Games",          "Jeux")        \
    X(HOME_APPS,       "home.apps",         "Applications",   "Applications")\
    X(HOME_FILES,      "home.files",        "File Manager",                  \
                                            "Gestionnaire de fichiers")      \
    X(HOME_DEVICES,    "home.devices",      "Devices",        "P\xc3\xa9riph\xc3\xa9riques") \
    X(HOME_VIDEO,      "home.video",        "Video",          "Vid\xc3\xa9o")\
    X(HOME_SETTINGS,   "home.settings",     "Settings",       "R\xc3\xa9glages") \
    X(HOME_SYSINFO,    "home.sysinfo",      "System Info",                   \
                                            "Informations syst\xc3\xa8me")   \
    X(HOME_POWER,      "home.power",        "Power",          "Alimentation")\
    X(HOME_D_GAMES,    "home.d.games",                                       \
        "Browse and launch games from your devices.",                        \
        "Parcourir et lancer les jeux de vos p\xc3\xa9riph\xc3\xa9riques.")   \
    X(HOME_D_FILES,    "home.d.files",                                       \
        "Copy, move and delete files across devices.",                       \
        "Copier, d\xc3\xa9placer et supprimer des fichiers.")                 \
    X(HOME_D_VIDEO,    "home.d.video",                                       \
        "Display mode, aspect ratio and screen position.",                   \
        "Mode d'affichage, format d'image et position de l'\xc3\xa9""cran.") \
    X(HOME_D_SETTINGS, "home.d.settings",                                    \
        "Language, controls, devices and updates.",                          \
        "Langue, commandes, p\xc3\xa9riph\xc3\xa9riques et mises \xc3\xa0 jour.") \
    /* Devices. */                                                           \
    X(DEV_READY,       "dev.ready",         "Ready",          "Pr\xc3\xaat") \
    X(DEV_UNFORMATTED, "dev.unformatted",   "Not formatted",                 \
                                            "Non format\xc3\xa9")            \
    X(DEV_ERROR,       "dev.error",         "Error",          "Erreur")      \
    X(DEV_ABSENT,      "dev.absent",        "Not connected",                 \
                                            "Non connect\xc3\xa9")           \
    X(DEV_FREE_KB,     "dev.free_kb",       "%d KB free",                    \
                                            "%d Ko libres")                  \
    X(DEV_FREE_UNKNOWN,"dev.free_unknown",  "free space unknown",            \
                                            "espace libre inconnu")          \
    /* Applications. */                                                      \
    X(APPS_EMPTY,      "apps.empty",        "No applications found.",        \
                                            "Aucune application trouv\xc3\xa9""e.") \
    X(APPS_EMPTY_HINT, "apps.empty.hint",                                    \
        "Copy an .ELF into one of these folders:",                           \
        "Copiez un fichier .ELF dans l'un de ces dossiers :")                \
    X(APPS_EMPTY_META, "apps.empty.meta",                                    \
        "A folder with an app.ini can set the name; "                        \
        "otherwise the filename is used.",                                   \
        "Un dossier contenant un app.ini peut d\xc3\xa9""finir le nom ; "    \
        "sinon le nom du fichier est utilis\xc3\xa9.")                       \
    X(APPS_FAIL_TITLE, "apps.fail.title",   "Could not launch",              \
                                            "Lancement impossible")          \
    X(APPS_FAIL_GONE,  "apps.fail.gone",                                     \
        "The file is gone. Was the device removed?",                         \
        "Le fichier a disparu. Le p\xc3\xa9riph\xc3\xa9rique a-t-il "        \
        "\xc3\xa9t\xc3\xa9 retir\xc3\xa9 ?")                                 \
    X(APPS_FAIL_FORMAT,"apps.fail.format",                                   \
        "This file is not a PS2 program.",                                   \
        "Ce fichier n'est pas un programme PS2.")                            \
    X(APPS_FAIL_OTHER, "apps.fail.other",                                    \
        "The program could not be started.",                                 \
        "Le programme n'a pas pu \xc3\xaatre d\xc3\xa9marr\xc3\xa9.")        \
    /* System information. */                                                \
    X(SYS_VERSION,     "sys.version",       "Version",        "Version")     \
    X(SYS_VIDEO_MODE,  "sys.video_mode",    "Video mode",     "Mode vid\xc3\xa9o") \
    X(SYS_RESOLUTION,  "sys.resolution",    "Resolution",     "R\xc3\xa9solution") \
    X(SYS_ASPECT,      "sys.aspect",        "Aspect",         "Format d'image") \
    X(SYS_AVAILABLE,   "sys.available",     "available",      "disponible")  \
    X(SYS_UNAVAILABLE, "sys.unavailable",   "unavailable",    "indisponible")\
    /* Power. */                                                             \
    X(POWER_BROWSER,   "power.browser",     "Return to PS2 Browser",         \
                                            "Retour au navigateur PS2")      \
    X(POWER_D_BROWSER, "power.d.browser",                                    \
        "Leave AtlasPS2 and start the console browser.",                     \
        "Quitter AtlasPS2 et d\xc3\xa9marrer le navigateur de la console.")  \
    X(POWER_OFF,       "power.off",         "Power Off",      "\xc3\x89teindre") \
    X(POWER_D_OFF,     "power.d.off",       "Shut the console down.",        \
                                            "\xc3\x89teindre la console.")   \
    /* Placeholder screen. */                                                \
    X(TODO_TITLE,      "todo.title",        "Coming soon",    "\xc3\x80 venir") \
    X(TODO_BODY,       "todo.body",         "Not implemented yet in this build.", \
                                            "Pas encore disponible dans cette version.") \
    /* Settings values, shown wherever a language is chosen. */              \
    X(LANG_EN,         "lang.en",           "English",        "Anglais")     \
    X(LANG_FR,         "lang.fr",           "French",         "Fran\xc3\xa7""ais")

typedef enum {
#define ATLAS_STR_ENUM(id, name, en, fr) ATLAS_STR_##id,
    ATLAS_STR_LIST(ATLAS_STR_ENUM)
#undef ATLAS_STR_ENUM
    ATLAS_STR_COUNT
} atlas_str_id_t;

/** The languages that ship built in. */
typedef enum {
    ATLAS_LANG_EN = 0,
    ATLAS_LANG_FR,
    ATLAS_LANG_COUNT
} atlas_lang_t;

/**
 * Look up a string in the active language.
 *
 * Never returns NULL, and never returns "": a key whose override file
 * supplied nothing falls back to the built-in text, and an out-of-range
 * id returns "?". A blank label is a control the user cannot identify,
 * which is worse than one in the wrong language.
 */
const char *atlas_str(atlas_str_id_t id);

/** The active language. Defaults to English until one is chosen. */
atlas_lang_t atlas_i18n_lang(void);

/**
 * Switch language. Drops any loaded overrides, since they belonged to
 * the previous language.
 */
void atlas_i18n_set_lang(atlas_lang_t lang);

/** Two-letter code ("en", "fr") for writing into ATLAS.INI. */
const char *atlas_i18n_lang_code(atlas_lang_t lang);

/**
 * Parse a two-letter code. Unknown codes give English rather than
 * failing: a configuration naming a language we do not have should
 * still boot into a usable interface.
 */
atlas_lang_t atlas_i18n_lang_from_code(const char *code);

/**
 * Apply one key/value pair from a translation file.
 *
 * Exposed for the file loader and for the self-checks. An unknown key
 * is ignored - a file written for a later version must not be rejected
 * wholesale by an older one.
 *
 * @return 1 if the key was known and stored, 0 otherwise.
 */
int atlas_i18n_set(const char *name, const char *value);

/** Forget every override, returning to the built-in tables. */
void atlas_i18n_clear_overrides(void);

/**
 * Load `lang/<code>.ini` from the first device that has one.
 *
 * Failure is silent and harmless: the built-in table is already
 * correct, and a missing file is the normal case.
 */
void atlas_i18n_load_overrides(void);

/** The file-facing name of `id`, e.g. "home.games". For the generator. */
const char *atlas_i18n_key_name(atlas_str_id_t id);

/** The built-in text of `id` in `lang`, ignoring overrides. */
const char *atlas_i18n_builtin(atlas_lang_t lang, atlas_str_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_I18N_H */
