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
    X(CHANGE,          "action.change",     "Change",         "Modifier")    \
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
    /*                                                                       \
     * Favorites. These name the three groups the applications list is       \
     * drawn in, so they are headings rather than commands - except the      \
     * footer hint, which is what Square does to the highlighted row.        \
     */                                                                      \
    X(FAV_FAVORITES,   "fav.favorites",     "Favorites",      "Favoris")     \
    X(FAV_RECENT,      "fav.recent",        "Recently used",                 \
                                            "R\xc3\xa9""cemment utilis\xc3\xa9") \
    X(FAV_ALL,         "fav.all",           "All applications",              \
                                            "Toutes les applications")       \
    X(FAV_TOGGLE,      "fav.toggle",        "Favorite",       "Favori")      \
    /*                                                                       \
     * File manager. The confirmations are written to name the file and      \
     * say what will happen to it - "Are you sure?" over a dialog that       \
     * does not repeat the target is how the wrong thing gets deleted.       \
     */                                                                      \
    X(FM_TITLE,        "fm.title",          "File Manager",                  \
                                            "Gestionnaire de fichiers")      \
    X(FM_EMPTY,        "fm.empty",          "This folder is empty.",         \
                                            "Ce dossier est vide.")          \
    X(FM_UP,           "fm.up",             "Up one level",                  \
                                            "Dossier parent")                \
    X(FM_TRUNCATED,    "fm.truncated",                                       \
        "Too many files to show them all.",                                  \
        "Trop de fichiers pour tous les afficher.")                          \
    X(FM_UNREADABLE,   "fm.unreadable",     "This folder cannot be read.",   \
                                            "Ce dossier ne peut pas "        \
                                            "\xc3\xaatre lu.")               \
    X(FM_MENU,         "fm.menu",           "Actions",        "Actions")     \
    X(FM_OPEN,         "fm.open",           "Open",           "Ouvrir")      \
    X(FM_LAUNCH,       "fm.launch",         "Launch",         "Lancer")      \
    X(FM_COPY,         "fm.copy",           "Copy",           "Copier")      \
    X(FM_MOVE,         "fm.move",           "Move",           "D\xc3\xa9placer") \
    X(FM_PASTE,        "fm.paste",          "Paste here",     "Coller ici")  \
    X(FM_DELETE,       "fm.delete",         "Delete",         "Supprimer")   \
    X(FM_MKDIR,        "fm.mkdir",          "New folder",     "Nouveau dossier") \
    X(FM_RENAME,       "fm.rename",         "Rename",         "Renommer")    \
    X(FM_CLIPBOARD,    "fm.clipboard",      "Ready to paste:",               \
                                            "\xc3\x80 coller :")             \
    X(FM_D_COPY,       "fm.d.copy",                                          \
        "Choose a folder, then Paste here.",                                 \
        "Choisissez un dossier, puis Coller ici.")                           \
    X(FM_Q_DELETE,     "fm.q.delete",       "Delete this file?",             \
                                            "Supprimer ce fichier ?")        \
    X(FM_Q_DELETE_DIR, "fm.q.delete_dir",   "Delete this folder?",           \
                                            "Supprimer ce dossier ?")        \
    X(FM_Q_OVERWRITE,  "fm.q.overwrite",                                     \
        "A file with this name is already here. Replace it?",                \
        "Un fichier de ce nom existe d\xc3\xa9j\xc3\xa0 ici. "               \
        "Le remplacer ?")                                                    \
    X(FM_W_SYSTEM,     "fm.w.system",                                        \
        "This is a system file. Deleting it can stop the console "           \
        "from starting.",                                                    \
        "Ceci est un fichier syst\xc3\xa8me. Le supprimer peut "             \
        "emp\xc3\xaa""cher la console de d\xc3\xa9marrer.")                  \
    X(FM_W_AGAIN,      "fm.w.again",        "Delete it anyway?",             \
                                            "Le supprimer quand m\xc3\xaame ?") \
    X(FM_WORKING,      "fm.working",        "Working...",     "En cours...") \
    X(FM_OK_DELETE,    "fm.ok.delete",      "Deleted.",       "Supprim\xc3\xa9.") \
    X(FM_OK_COPY,      "fm.ok.copy",        "Copied.",        "Copi\xc3\xa9.") \
    X(FM_OK_MOVE,      "fm.ok.move",        "Moved.",         "D\xc3\xa9plac\xc3\xa9.") \
    X(FM_OK_MKDIR,     "fm.ok.mkdir",       "Folder created.",               \
                                            "Dossier cr\xc3\xa9\xc3\xa9.")   \
    X(FM_E_DELETE,     "fm.e.delete",       "It could not be deleted.",      \
                                            "Suppression impossible.")       \
    X(FM_E_NOTEMPTY,   "fm.e.notempty",                                      \
        "This folder is not empty. Empty it first.",                         \
        "Ce dossier n'est pas vide. Videz-le d'abord.")                      \
    X(FM_E_COPY,       "fm.e.copy",         "The copy failed. "              \
                                            "Nothing was changed here.",     \
                                            "La copie a \xc3\xa9""chou\xc3\xa9. " \
                                            "Rien n'a \xc3\xa9t\xc3\xa9 modifi\xc3\xa9 ici.") \
    X(FM_E_MOVE,       "fm.e.move",         "The move failed. "              \
                                            "The original is still there.",  \
                                            "Le d\xc3\xa9placement a "       \
                                            "\xc3\xa9""chou\xc3\xa9. "       \
                                            "L'original est intact.")        \
    X(FM_E_MKDIR,      "fm.e.mkdir",        "The folder could not be "       \
                                            "created.",                     \
                                            "Le dossier n'a pas pu "         \
                                            "\xc3\xaatre cr\xc3\xa9\xc3\xa9.") \
    X(FM_E_SAME,       "fm.e.same",         "The source and the "            \
                                            "destination are the same.",     \
                                            "La source et la destination "   \
                                            "sont identiques.")              \
    X(FM_E_NAME,       "fm.e.name",         "That name cannot be used "      \
                                            "here.",                        \
                                            "Ce nom ne peut pas "            \
                                            "\xc3\xaatre utilis\xc3\xa9 ici.") \
    X(FM_NEWDIR_NAME,  "fm.newdir_name",    "New folder",     "Nouveau dossier") \
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
    X(LANG_FR,         "lang.fr",           "French",         "Fran\xc3\xa7""ais") \
    /* ------------------------------------------------------------------ */\
    /* Installer.                                                          */\
    /*                                                                     */\
    /* Built into the same table as the launcher's, and linked into both   */\
    /* programs. The installer runs on a console whose Memory Card may be  */\
    /* the thing that is broken, so it cannot depend on a language file    */\
    /* living on that card - which is the same reason the tables are built */\
    /* in at all.                                                          */\
    /* ------------------------------------------------------------------ */\
    X(INS_TITLE,       "ins.title",         "AtlasPS2 Installer",            \
                                            "Installateur AtlasPS2")         \
    X(INS_CONSOLE,     "ins.console",       "Console",        "Console")     \
    X(INS_TARGET,      "ins.target",        "Target",         "Destination") \
    X(INS_FREE,        "ins.free",          "Free space",     "Espace libre")\
    X(INS_SOURCE,      "ins.source",        "Source",         "Source")      \
    X(INS_NONE,        "ins.none",          "not found",      "introuvable") \
    X(INS_INSTALLED,   "ins.installed",     "AtlasPS2 installed",            \
                                            "AtlasPS2 install\xc3\xa9")      \
    X(INS_NOT_INST,    "ins.not_installed", "AtlasPS2 not installed",        \
                                            "AtlasPS2 non install\xc3\xa9")  \
    X(INS_CHANGE_CARD, "ins.change_card",   "Change card",                   \
                                            "Changer de carte")              \
    /* Menu entries and what each one actually does. */                      \
    X(INS_INSTALL,     "ins.install",       "Install AtlasPS2",              \
                                            "Installer AtlasPS2")            \
    X(INS_D_INSTALL,   "ins.d.install",                                      \
        "Copy AtlasPS2 onto this Memory Card.",                              \
        "Copier AtlasPS2 sur cette carte m\xc3\xa9moire.")                    \
    X(INS_UPDATE,      "ins.update",        "Update AtlasPS2",               \
                                            "Mettre \xc3\xa0 jour AtlasPS2") \
    X(INS_D_UPDATE,    "ins.d.update",                                       \
        "Replace the program. Your settings are kept.",                      \
        "Remplacer le programme. Vos r\xc3\xa9glages sont conserv\xc3\xa9s.") \
    X(INS_REPAIR,      "ins.repair",        "Repair installation",           \
                                            "R\xc3\xa9parer l'installation") \
    X(INS_D_REPAIR,    "ins.d.repair",                                       \
        "Copy the program again and check every file.",                      \
        "Recopier le programme et v\xc3\xa9rifier chaque fichier.")           \
    X(INS_BACKUP,      "ins.backup",        "Backup existing setup",         \
                                            "Sauvegarder l'installation actuelle") \
    X(INS_D_BACKUP,    "ins.d.backup",                                       \
        "Keep a copy of the boot program that is there now.",                \
        "Conserver une copie du programme de d\xc3\xa9marrage actuel.")       \
    X(INS_RESTORE,     "ins.restore",       "Restore backup",                \
                                            "Restaurer la sauvegarde")       \
    X(INS_D_RESTORE,   "ins.d.restore",                                      \
        "Put the saved boot program back in place.",                         \
        "Remettre en place le programme de d\xc3\xa9marrage sauvegard\xc3\xa9.") \
    X(INS_UNINSTALL,   "ins.uninstall",     "Uninstall AtlasPS2",            \
                                            "D\xc3\xa9sinstaller AtlasPS2")  \
    X(INS_D_UNINSTALL, "ins.d.uninstall",                                    \
        "Restore the boot program that was there before AtlasPS2.",          \
        "Restaurer le programme de d\xc3\xa9marrage pr\xc3\xa9""c\xc3\xa9""dent.") \
    X(INS_EXIT,        "ins.exit",          "Exit",           "Quitter")     \
    X(INS_D_EXIT,      "ins.d.exit",                                         \
        "Leave the installer without changing anything.",                    \
        "Quitter l'installateur sans rien modifier.")                        \
    /* The progress list. Deliberately neutral: these same five lines are  */\
    /* shown for an uninstall, where "Installing AtlasPS2" would be a lie. */\
    X(INS_STEP_CHECK,  "ins.step.check",    "Preparing Memory Card",         \
                                            "Pr\xc3\xa9paration de la carte")\
    X(INS_STEP_BACKUP, "ins.step.backup",   "Creating backup",               \
                                            "Cr\xc3\xa9""ation de la sauvegarde") \
    X(INS_STEP_COPY,   "ins.step.copy",     "Copying program",               \
                                            "Copie du programme")            \
    X(INS_STEP_CONFIG, "ins.step.config",   "Installing configuration",      \
                                            "Installation de la configuration") \
    X(INS_STEP_VERIFY, "ins.step.verify",   "Verifying files",               \
                                            "V\xc3\xa9rification des fichiers") \
    X(INS_WORKING,     "ins.working",                                        \
        "Working. Do not remove the Memory Card.",                           \
        "En cours. Ne retirez pas la carte m\xc3\xa9moire.")                  \
    /* Outcomes. */                                                          \
    X(INS_OK_INSTALL,  "ins.ok.install",    "Installation completed.",       \
                                            "Installation termin\xc3\xa9""e.") \
    X(INS_OK_UPDATE,   "ins.ok.update",     "Update completed.",             \
                                            "Mise \xc3\xa0 jour termin\xc3\xa9""e.") \
    X(INS_OK_REPAIR,   "ins.ok.repair",     "Installation repaired.",        \
                                            "Installation r\xc3\xa9par\xc3\xa9""e.") \
    X(INS_OK_BACKUP,   "ins.ok.backup",     "Backup created.",               \
                                            "Sauvegarde cr\xc3\xa9\xc3\xa9""e.") \
    X(INS_OK_RESTORE,  "ins.ok.restore",    "Backup restored.",              \
                                            "Sauvegarde restaur\xc3\xa9""e.") \
    X(INS_OK_UNINSTALL,"ins.ok.uninstall",  "AtlasPS2 was removed.",         \
                                            "AtlasPS2 a \xc3\xa9t\xc3\xa9 supprim\xc3\xa9.") \
    X(INS_OK_ROLLBACK, "ins.ok.rollback",                                    \
        "The previous version was put back.",                                \
        "La version pr\xc3\xa9""c\xc3\xa9""dente a \xc3\xa9t\xc3\xa9 remise " \
        "en place.")                                                         \
    X(INS_RESTART,     "ins.restart",                                        \
        "Restart the console to use AtlasPS2.",                              \
        "Red\xc3\xa9marrez la console pour utiliser AtlasPS2.")               \
    /* Refusals and failures. Each says what state the card is in, because */\
    /* "it failed" leaves the user unable to tell whether to try again.    */\
    X(INS_E_NOCARD,    "ins.e.nocard",                                       \
        "No usable Memory Card in that slot.",                               \
        "Aucune carte m\xc3\xa9moire utilisable dans ce port.")               \
    X(INS_E_NOSRC,     "ins.e.nosrc",                                        \
        "ATLASPS2.ELF was not found next to the installer.",                 \
        "ATLASPS2.ELF est introuvable \xc3\xa0 c\xc3\xb4t\xc3\xa9 de l'installateur.") \
    X(INS_E_SPACE,     "ins.e.space",                                        \
        "Not enough free space on the Memory Card.",                         \
        "Espace insuffisant sur la carte m\xc3\xa9moire.")                    \
    X(INS_E_COPY,      "ins.e.copy",                                         \
        "The program could not be copied.",                                  \
        "Le programme n'a pas pu \xc3\xaatre copi\xc3\xa9.")                  \
    X(INS_E_VERIFY,    "ins.e.verify",                                       \
        "The copy did not match the original. Nothing was changed.",         \
        "La copie ne correspond pas \xc3\xa0 l'original. Rien n'a "           \
        "\xc3\xa9t\xc3\xa9 modifi\xc3\xa9.")                                  \
    X(INS_E_ROLLBACK,  "ins.e.rollback",                                     \
        "It failed. The previous program was put back.",                     \
        "\xc3\x89""chec. Le programme pr\xc3\xa9""c\xc3\xa9""dent a "         \
        "\xc3\xa9t\xc3\xa9 remis en place.")                                  \
    X(INS_E_NOBACKUP,  "ins.e.nobackup",                                     \
        "There is no backup on this Memory Card.",                           \
        "Aucune sauvegarde sur cette carte m\xc3\xa9moire.")                  \
    X(INS_E_NOTINST,   "ins.e.notinst",                                      \
        "AtlasPS2 is not installed on this Memory Card.",                    \
        "AtlasPS2 n'est pas install\xc3\xa9 sur cette carte m\xc3\xa9moire.") \
    X(INS_E_OTHER,     "ins.e.other",                                        \
        "The operation could not be completed.",                             \
        "L'op\xc3\xa9ration n'a pas pu \xc3\xaatre men\xc3\xa9""e \xc3\xa0 "   \
        "son terme.")                                                        \
    /* The bootstrap refusal. This is a safety message, not a limitation   */\
    /* to be apologised for: guessing a variant is what costs a user their */\
    /* Memory Card, so the program says plainly that it will not guess.    */\
    X(INS_BOOT_TITLE,  "ins.boot.title",    "Bootstrap not installed",       \
                                            "Bootstrap non install\xc3\xa9") \
    X(INS_BOOT_BODY,   "ins.boot.body",                                      \
        "This installer never installs a bootstrap or exploit. Which one a " \
        "console needs depends on its model and ROM, and the wrong one can " \
        "leave a Memory Card the console will not boot from. Set your "      \
        "exploit up with a tool made for it, then come back here.",          \
        "Cet installateur n'installe jamais de bootstrap ni d'exploit. "     \
        "Celui qu'il faut d\xc3\xa9pend du mod\xc3\xa8le et de la ROM de la " \
        "console, et un mauvais choix peut rendre la carte m\xc3\xa9moire "  \
        "ind\xc3\xa9marrable. Installez votre exploit avec un outil "        \
        "pr\xc3\xa9vu pour cela, puis revenez ici.")                          \
                                                                             \
    /* ------------------------------------------------------------------ */\
    /* Recovery.                                                           */\
    /*                                                                     */\
    /* Reached by holding L1+R1 at boot, and read by someone whose         */\
    /* console is not working. Every line here is written for a user who   */\
    /* does not know what went wrong: the labels say what will happen to   */\
    /* their console, not what the code does. "Reset configuration" is a   */\
    /* file operation; "Your settings return to their defaults" is what    */\
    /* the user needs to decide with.                                      */\
    /* ------------------------------------------------------------------ */\
    X(REC_TITLE,       "rec.title",         "AtlasPS2 Recovery",             \
                                            "R\xc3\xa9""cup\xc3\xa9ration AtlasPS2") \
    X(REC_INTRO,       "rec.intro",                                          \
        "Started in recovery mode. No settings or theme were loaded.",       \
        "D\xc3\xa9marr\xc3\xa9 en mode r\xc3\xa9""cup\xc3\xa9ration. Aucun "  \
        "r\xc3\xa9glage ni th\xc3\xa8me n'a \xc3\xa9t\xc3\xa9 charg\xc3\xa9.") \
    X(REC_CONTINUE,    "rec.continue",      "Start AtlasPS2 normally",       \
                                            "D\xc3\xa9marrer AtlasPS2 normalement") \
    X(REC_D_CONTINUE,  "rec.d.continue",                                     \
        "Leave recovery and open the normal interface.",                     \
        "Quitter la r\xc3\xa9""cup\xc3\xa9ration et ouvrir l'interface "     \
        "normale.")                                                          \
    X(REC_RESET_CFG,   "rec.reset_cfg",     "Reset configuration",           \
                                            "R\xc3\xa9initialiser la configuration") \
    X(REC_D_RESET_CFG, "rec.d.reset_cfg",                                    \
        "Settings return to their defaults. The old file is kept as "        \
        "ATLAS.INI.BAK.",                                                    \
        "Les r\xc3\xa9glages reviennent aux valeurs par d\xc3\xa9""faut. "   \
        "L'ancien fichier est conserv\xc3\xa9 sous ATLAS.INI.BAK.")           \
    X(REC_NO_THEME,    "rec.no_theme",      "Disable custom theme",          \
                                            "D\xc3\xa9sactiver le th\xc3\xa8me personnalis\xc3\xa9") \
    X(REC_D_NO_THEME,  "rec.d.no_theme",                                     \
        "Go back to the built-in appearance. Nothing is deleted.",           \
        "Revenir \xc3\xa0 l'apparence int\xc3\xa9gr\xc3\xa9""e. Rien n'est "  \
        "supprim\xc3\xa9.")                                                   \
    X(REC_ROLLBACK,    "rec.rollback",      "Restore previous version",      \
                                            "Restaurer la version pr\xc3\xa9""c\xc3\xa9""dente") \
    X(REC_D_ROLLBACK,  "rec.d.rollback",                                     \
        "Put back the AtlasPS2 build that was here before the last "         \
        "update.",                                                           \
        "Remettre la version d'AtlasPS2 pr\xc3\xa9sente avant la "           \
        "derni\xc3\xa8re mise \xc3\xa0 jour.")                                \
    X(REC_UPDATE,      "rec.update",        "Install update from USB",       \
                                            "Installer la mise \xc3\xa0 jour depuis l'USB") \
    X(REC_D_UPDATE,    "rec.d.update",                                       \
        "Copy ATLASPS2.ELF from mass:/ATLAS_UPDATE/ onto the Memory Card.",  \
        "Copier ATLASPS2.ELF depuis mass:/ATLAS_UPDATE/ sur la carte "       \
        "m\xc3\xa9moire.")                                                    \
    X(REC_BROWSER,     "rec.browser",       "Return to PS2 Browser",         \
                                            "Retourner au navigateur PS2")   \
    X(REC_D_BROWSER,   "rec.d.browser",                                      \
        "Leave AtlasPS2 and go back to the console's own menu.",             \
        "Quitter AtlasPS2 et revenir au menu de la console.")                \
    X(REC_TARGET,      "rec.target",        "Memory Card",   "Carte m\xc3\xa9moire") \
    X(REC_SWITCH,      "rec.switch",        "Change card",   "Changer de carte") \
    X(REC_D_SWITCH,    "rec.d.switch",                                       \
        "Work on the other Memory Card slot.",                               \
        "Travailler sur l'autre port de carte m\xc3\xa9moire.")               \
    X(REC_DONE_CFG,    "rec.done.cfg",      "Configuration reset.",          \
                                            "Configuration r\xc3\xa9initialis\xc3\xa9""e.") \
    X(REC_DONE_THEME,  "rec.done.theme",    "Built-in appearance restored.", \
                                            "Apparence int\xc3\xa9gr\xc3\xa9""e r\xc3\xa9tablie.") \
    X(REC_E_FAILED,    "rec.e.failed",                                       \
        "That did not work. Nothing was changed.",                           \
        "Cela n'a pas fonctionn\xc3\xa9. Rien n'a \xc3\xa9t\xc3\xa9 "        \
        "modifi\xc3\xa9.")                                                    \
    X(REC_E_NOUSB,     "rec.e.nousb",                                        \
        "No update found in mass:/ATLAS_UPDATE/.",                           \
        "Aucune mise \xc3\xa0 jour trouv\xc3\xa9""e dans "                    \
        "mass:/ATLAS_UPDATE/.")                                              \
                                                                             \
    /* ------------------------------------------------------------------ */\
    /* Video settings.                                                     */\
    /*                                                                     */\
    /* The values themselves - "auto", "ntsc", "pal", "480p", "4:3" - are  */\
    /* not here. They are the same tokens in both languages and the same   */\
    /* tokens that go into ATLAS.INI, so translating them would mean a     */\
    /* user reading a French screen could not find what they saw in the    */\
    /* file.                                                               */\
    /* ------------------------------------------------------------------ */\
    X(VID_MODE,        "vid.mode",          "Display mode",   "Mode d'affichage") \
    X(VID_D_MODE,      "vid.d.mode",                                         \
        "AUTO follows the console's region. 480p needs a component or VGA "  \
        "cable.",                                                            \
        "AUTO suit la r\xc3\xa9gion de la console. Le 480p exige un c\xc3\xa2""ble " \
        "composantes ou VGA.")                                               \
    X(VID_ASPECT,      "vid.aspect",        "Aspect ratio",   "Format d'image") \
    X(VID_D_ASPECT,    "vid.d.aspect",                                       \
        "Changes how the interface is drawn. It does not change how a "      \
        "game runs.",                                                        \
        "Change le dessin de l'interface. Cela ne change rien au "           \
        "fonctionnement d'un jeu.")                                          \
    X(VID_OFFSET_X,    "vid.offset_x",      "Horizontal position",           \
                                            "Position horizontale")          \
    X(VID_OFFSET_Y,    "vid.offset_y",      "Vertical position",             \
                                            "Position verticale")            \
    X(VID_D_OFFSET,    "vid.d.offset",                                       \
        "Moves the picture on a television where it sits off-centre.",       \
        "D\xc3\xa9place l'image sur un t\xc3\xa9l\xc3\xa9viseur o\xc3\xb9 elle est " \
        "d\xc3\xa9""centr\xc3\xa9""e.")                                       \
    X(VID_OVERSCAN_X,  "vid.overscan_x",    "Horizontal margin",             \
                                            "Marge horizontale")             \
    X(VID_OVERSCAN_Y,  "vid.overscan_y",    "Vertical margin",               \
                                            "Marge verticale")               \
    X(VID_D_OVERSCAN,  "vid.d.overscan",                                     \
        "Extra margin for a television that cuts off the edges.",            \
        "Marge suppl\xc3\xa9mentaire pour un t\xc3\xa9l\xc3\xa9viseur qui rogne " \
        "les bords.")                                                        \
    X(VID_RESET,       "vid.reset",         "Reset to defaults",             \
                                            "R\xc3\xa9tablir les valeurs par d\xc3\xa9""faut") \
    X(VID_D_RESET,     "vid.d.reset",                                        \
        "Back to AUTO with no adjustments. Nothing is written yet.",         \
        "Retour \xc3\xa0 AUTO sans aucun r\xc3\xa9glage. Rien n'est encore "  \
        "\xc3\xa9""crit.")                                                    \
    X(VID_SAVE,        "vid.save",          "Save",           "Enregistrer") \
    X(VID_D_SAVE,      "vid.d.save",                                         \
        "Write these settings to ATLAS.INI so they survive a restart.",      \
        "\xc3\x89""crire ces r\xc3\xa9glages dans ATLAS.INI pour qu'ils "     \
        "survivent \xc3\xa0 un red\xc3\xa9marrage.")                          \
    X(VID_KEEP,        "vid.keep",          "Keep this display mode?",       \
                                            "Conserver ce mode d'affichage ?") \
    X(VID_KEEP_BODY,   "vid.keep.body",                                      \
        "Going back to the previous mode in %d s. If you cannot read "       \
        "this, wait.",                                                       \
        "Retour au mode pr\xc3\xa9""c\xc3\xa9""dent dans %d s. Si vous ne "   \
        "lisez pas ceci, attendez.")                                         \
    X(VID_REVERTED,    "vid.reverted",      "Previous display mode restored.", \
                                            "Mode d'affichage pr\xc3\xa9""c\xc3\xa9""dent r\xc3\xa9tabli.") \
    X(VID_SAVED,       "vid.saved",         "Video settings saved.",         \
                                            "R\xc3\xa9glages vid\xc3\xa9o enregistr\xc3\xa9s.") \
    X(VID_E_SAVE,      "vid.e.save",                                         \
        "Could not save. No device could be written to.",                    \
        "Enregistrement impossible. Aucun p\xc3\xa9riph\xc3\xa9rique n'a pu " \
        "\xc3\xaatre \xc3\xa9""crit.")                                        \
    X(VID_NOTE,        "vid.note",                                           \
        "Output signal only: the interface is always drawn at 640x448 or "   \
        "640x512.",                                                          \
        "Signal de sortie uniquement : l'interface est toujours dessin\xc3\xa9""e " \
        "en 640x448 ou 640x512.")                                            \
                                                                             \
    /* ------------------------------------------------------------------ */\
    /* Themes.                                                             */\
    /*                                                                     */\
    /* A theme's own name is not translated: it is the name of a folder    */\
    /* on the card, and a user looking for it there has to find the same   */\
    /* word they saw on the screen.                                        */\
    /* ------------------------------------------------------------------ */\
    X(THEME_TITLE,     "theme.title",       "Theme",          "Th\xc3\xa8me") \
    X(THEME_BUILTIN,   "theme.builtin",     "Built-in",       "Int\xc3\xa9gr\xc3\xa9") \
    X(THEME_D_BUILTIN, "theme.d.builtin",                                    \
        "The default AtlasPS2 look. Always available.",                      \
        "L'apparence AtlasPS2 par d\xc3\xa9""faut. Toujours disponible.")     \
    X(THEME_D_ONE,     "theme.d.one",                                        \
        "Applied straight away. Choose Save to keep it after a restart.",    \
        "Appliqu\xc3\xa9 imm\xc3\xa9""diatement. Choisissez Enregistrer pour " \
        "le conserver apr\xc3\xa8s un red\xc3\xa9marrage.")                   \
    X(THEME_SAVE,      "theme.save",        "Save",           "Enregistrer") \
    X(THEME_D_SAVE,    "theme.d.save",                                       \
        "Write the chosen theme to ATLAS.INI.",                              \
        "\xc3\x89""crire le th\xc3\xa8me choisi dans ATLAS.INI.")             \
    X(THEME_SAVED,     "theme.saved",       "Theme saved.",                  \
                                            "Th\xc3\xa8me enregistr\xc3\xa9.") \
    X(THEME_E_SAVE,    "theme.e.save",                                       \
        "Could not save. No device could be written to.",                    \
        "Enregistrement impossible. Aucun p\xc3\xa9riph\xc3\xa9rique n'a pu " \
        "\xc3\xaatre \xc3\xa9""crit.")                                        \
    X(THEME_E_LOAD,    "theme.e.load",                                       \
        "That theme could not be read. The built-in one is still in use.",   \
        "Ce th\xc3\xa8me n'a pas pu \xc3\xaatre lu. Le th\xc3\xa8me int\xc3\xa9gr\xc3\xa9 " \
        "reste actif.")                                                      \
    X(THEME_NONE,      "theme.none",                                         \
        "No themes found on the attached devices.",                          \
        "Aucun th\xc3\xa8me trouv\xc3\xa9 sur les p\xc3\xa9riph\xc3\xa9riques " \
        "connect\xc3\xa9s.")                                                  \
    X(THEME_NONE_HINT, "theme.none.hint",                                    \
        "A theme is a folder holding theme.ini, in ATLAS/THEMES on a "       \
        "Memory Card or a USB stick.",                                       \
        "Un th\xc3\xa8me est un dossier contenant theme.ini, dans "           \
        "ATLAS/THEMES sur une carte m\xc3\xa9moire ou une cl\xc3\xa9 USB.")   \
    X(THEME_NOTE,      "theme.note",                                         \
        "The built-in theme cannot be removed, so a missing theme file "     \
        "never leaves the console unusable.",                                \
        "Le th\xc3\xa8me int\xc3\xa9gr\xc3\xa9 ne peut pas \xc3\xaatre "       \
        "supprim\xc3\xa9 : un fichier de th\xc3\xa8me manquant ne rend "      \
        "jamais la console inutilisable.")

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
