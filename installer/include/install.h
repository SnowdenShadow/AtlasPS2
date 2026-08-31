/*
 * AtlasPS2 - install.h
 *
 * The installer's engine: everything that touches a Memory Card, with
 * no drawing in it. The screen above drives it one step per frame and
 * renders whatever the job structure says.
 *
 * WHY THIS IS A SEPARATE PROGRAM
 * ------------------------------
 * The installer writes the file the console boots. If it were a screen
 * inside AtlasPS2, then repairing a broken installation would mean
 * running the broken installation - and a launcher that cannot start is
 * exactly when a user needs the installer most. So it ships as its own
 * ELF, launched from USB by whatever homebrew environment the console
 * already has.
 *
 * WHAT IT DELIBERATELY DOES NOT DO
 * --------------------------------
 * It never installs a bootstrap or exploit. Which variant a console
 * needs depends on its ROM version and region, getting it wrong can
 * leave a Memory Card that the console refuses to boot from, and there
 * is no reliable way to determine the right one from software on every
 * model. Guessing is the failure mode that costs a user their card, so
 * this program stops and says so instead. Installing AtlasPS2 onto a
 * console that already runs homebrew is a separate, safe operation, and
 * that is the only one offered here.
 *
 * THE TWO KINDS OF BACKUP
 * -----------------------
 * They are not the same thing and conflating them silently destroys the
 * user's way back:
 *
 *   BOOT/BOOT.BAK          the rollback slot for one transaction. It is
 *                          overwritten by every install and update, and
 *                          exists so a failed swap can be undone within
 *                          seconds.
 *
 *   ATLAS/BACKUP/BOOT.ELF  whatever was booting the console BEFORE
 *                          AtlasPS2 was ever installed. Written once
 *                          and never overwritten, because after the
 *                          first update the rollback slot holds one of
 *                          our own builds - and an uninstall that
 *                          restored that would "remove" AtlasPS2 by
 *                          reinstalling it.
 */
#ifndef ATLAS_INSTALL_H
#define ATLAS_INSTALL_H

#include "atlas/atlas.h"
#include "atlas/device.h"
#include "atlas/i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Long enough for "mc0:/ATLAS/BACKUP/ATLAS.INI" many times over. */
#define ATLAS_INSTALL_PATH_MAX 128

/* ------------------------------------------------------------------ */
/* Operations                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    ATLAS_OP_INSTALL = 0,  /**< first-time install onto a card         */
    ATLAS_OP_UPDATE,       /**< replace the program, keep the settings */
    ATLAS_OP_REPAIR,       /**< re-copy and re-verify what is there    */
    ATLAS_OP_BACKUP,       /**< refresh ATLAS/BACKUP by hand           */
    ATLAS_OP_RESTORE,      /**< put ATLAS/BACKUP back into place       */
    ATLAS_OP_UNINSTALL,    /**< restore the previous boot program      */
    ATLAS_OP_COUNT
} atlas_install_op_t;

/* ------------------------------------------------------------------ */
/* Steps                                                               */
/*                                                                     */
/* One list for every operation, so the user sees the same five lines  */
/* whatever they picked and a skipped line is visibly skipped rather   */
/* than absent. The names are deliberately operation-neutral: a        */
/* progress list that reads "Installing AtlasPS2" during an uninstall  */
/* is worse than no list at all.                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    ATLAS_STEP_CHECK = 0,  /**< card usable, space available, source found */
    ATLAS_STEP_BACKUP,     /**< preserve what is already on the card       */
    ATLAS_STEP_COPY,       /**< stage the program under a temporary name   */
    ATLAS_STEP_CONFIG,     /**< settings and folder tree                   */
    ATLAS_STEP_VERIFY,     /**< check the copy, then make it the live one  */
    ATLAS_STEP_COUNT
} atlas_install_step_t;

typedef enum {
    ATLAS_STEP_PENDING = 0,
    ATLAS_STEP_RUNNING,
    ATLAS_STEP_DONE,
    ATLAS_STEP_SKIPPED,    /**< not part of this operation                 */
    ATLAS_STEP_FAILED
} atlas_install_step_state_t;

/** The label for `step`, in the active language. */
atlas_str_id_t atlas_install_step_label(atlas_install_step_t step);

/* ------------------------------------------------------------------ */
/* A job                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    atlas_install_op_t op;
    atlas_device_id_t  target;

    /** Where ATLASPS2.ELF was found. Empty for operations not needing it. */
    char source[ATLAS_INSTALL_PATH_MAX];

    atlas_install_step_state_t state[ATLAS_STEP_COUNT];

    /** Next step to run; ATLAS_STEP_COUNT once nothing is left. */
    int current;

    /** Set once the job has stopped, whether it succeeded or not. */
    int done;

    atlas_err_t    err;      /**< ATLAS_OK unless a step failed        */
    atlas_str_id_t message;  /**< what to tell the user, always valid  */
} atlas_install_job_t;

/**
 * Progress from inside a long step.
 *
 * A verified 700 KB copy to a Memory Card is three passes over the file
 * and takes long enough that a still screen reads as a hang - and the
 * reflex, pulling the card, is what turns a slow install into a broken
 * one. The engine has no idea how to draw, so it calls this instead;
 * the screen installs a function that renders one frame.
 *
 * @param total  bytes expected, or -1 when not known
 */
typedef void (*atlas_install_tick_fn)(const atlas_install_job_t *job,
                                      int done, int total, void *ctx);

void atlas_install_set_tick(atlas_install_tick_fn fn, void *ctx);

/**
 * Set up a job. Does not touch the card - nothing is written until
 * atlas_install_pump() is called.
 *
 * A job that cannot run at all (no source ELF, unusable card) comes
 * back already `done` with `err` and `message` set, so the caller has
 * one place to look for a failure rather than two.
 */
void atlas_install_begin(atlas_install_job_t *job, atlas_install_op_t op,
                         atlas_device_id_t target);

/**
 * Run the next step.
 *
 * One step per call so the caller can draw between them: this is what
 * makes the checkmarks appear one at a time instead of all at once
 * after a frozen screen.
 *
 * @return 1 while there is more to do, 0 once `job->done` is set.
 */
int atlas_install_pump(atlas_install_job_t *job);

/* ------------------------------------------------------------------ */
/* Inspection, for building the menu                                   */
/* ------------------------------------------------------------------ */

/** Is AtlasPS2 already the boot program on this device? */
int atlas_install_is_installed(atlas_device_id_t dev);

/** Does this device carry an ATLAS/BACKUP to restore from? */
int atlas_install_has_backup(atlas_device_id_t dev);

/**
 * Can `op` be performed on `dev` right now?
 *
 * The menu greys out what it cannot do rather than hiding it: an option
 * that vanishes leaves the user wondering whether the program has it at
 * all, while a greyed one plus its reason is an explanation.
 */
int atlas_install_op_available(atlas_install_op_t op, atlas_device_id_t dev);

/**
 * Find the ATLASPS2.ELF this installer should copy.
 *
 * Looks beside itself first - the release ships the two files together,
 * so the stick the installer was launched from is where it will be.
 *
 * @return ATLAS_OK, or ATLAS_ENOENT with `out` emptied.
 */
atlas_err_t atlas_install_find_source(char *out, int size);

/**
 * The console's ROM name, e.g. "0160EC20030325".
 *
 * Shown to the user and recorded in the log, never acted on: it is
 * displayed so that a compatibility report has something concrete in
 * it, not so this program can decide anything from it.
 *
 * @param out  at least 16 bytes. Always NUL-terminated; set to "?" when
 *             the ROM could not be read.
 */
void atlas_install_console_id(char *out, int size);

/** Size in KB the operation needs free on the target, or 0 if none. */
int atlas_install_space_needed_kb(const atlas_install_job_t *job);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_INSTALL_H */
