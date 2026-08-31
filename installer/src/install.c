/*
 * AtlasPS2 - install.c
 * The installer engine: the part that writes to a Memory Card.
 *
 * Every operation here is built out of the same three guarantees:
 *
 *   1. Nothing is deleted that the user did not put there.
 *   2. The live BOOT.ELF is never opened for writing. A new program is
 *      staged under another name, verified, and only then renamed into
 *      place - so a failure at any moment leaves a bootable card.
 *   3. What was booting the console before AtlasPS2 arrived is copied
 *      aside once, and never overwritten afterwards. That copy is what
 *      an uninstall gives back.
 */
#include <string.h>
#include <stdio.h>

#include <rom0_info.h>

#include "install.h"

#include "atlas/config.h"
#include "atlas/device.h"
#include "atlas/file.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Layout on the card                                                  */
/*                                                                     */
/* BOOT/BOOT.ELF is the path a PS2 exploit chain looks for, so that is */
/* where the program goes. Everything else AtlasPS2 owns lives under   */
/* ATLAS/, well away from anything another boot environment created.   */
/* ------------------------------------------------------------------ */

#define P_BOOT_DIR    "BOOT"
#define P_BOOT        "BOOT/BOOT.ELF"
#define P_BOOT_NEW    "BOOT/BOOT.NEW"
#define P_BOOT_BAK    "BOOT/BOOT.BAK"

#define P_BACKUP_DIR  "ATLAS/BACKUP"
#define P_BACKUP_ELF  "ATLAS/BACKUP/BOOT.ELF"
#define P_BACKUP_MARK "ATLAS/BACKUP/ORIGINAL.TXT"

#define P_CFG_DIR     "ATLAS/CONFIG"
#define P_APPS_DIR    "ATLAS/APPS"
#define P_LANG_DIR    "ATLAS/LANG"
#define P_THEME_DIR   "ATLAS/THEMES"

/* The file the launcher is recognised by. A BOOT.ELF is just an ELF -
 * nothing in it says who wrote it - so installation is recorded next to
 * it instead of guessed from it. Without this, an uninstall could hand
 * back a card whose boot program belonged to something else entirely. */
#define P_MARKER      "ATLAS/CONFIG/INSTALLED.TXT"

/* Where ATLASPS2.ELF is looked for, in order. The release ships the two
 * ELFs side by side, so the stick the installer was launched from is
 * the first place to look; the rest cover a user who copied the files
 * into a folder or onto a card by hand. */
static const char *const s_source_rel[] = {
    "ATLASPS2.ELF",
    "ATLAS/ATLASPS2.ELF",
    "ATLAS/APPS/ATLASPS2.ELF"
};

/* Devices searched for the source, USB first: that is where a release
 * archive is unzipped, and a stale copy on a card should not win over
 * the one the user just plugged in. */
static const atlas_device_id_t s_source_dev[] = {
    ATLAS_DEV_MASS,
    ATLAS_DEV_MC0,
    ATLAS_DEV_MC1
};

/*
 * A Memory Card is 8 MB and a user's saves are on it, so the installer
 * refuses rather than filling it: 1024 KB covers a ~700 KB program plus
 * the staged copy's headroom and the small config files.
 */
#define INSTALL_MIN_FREE_KB 1024

/* ------------------------------------------------------------------ */
/* Progress                                                            */
/* ------------------------------------------------------------------ */

static atlas_install_tick_fn s_tick;
static void                 *s_tick_ctx;
static const atlas_install_job_t *s_tick_job;

void atlas_install_set_tick(atlas_install_tick_fn fn, void *ctx)
{
    s_tick = fn;
    s_tick_ctx = ctx;
}

/**
 * Bridge between the file layer's progress callback and the screen.
 *
 * Always returns 1: there is no cancel button during a write. Offering
 * one would mean stopping midway through the only operation on this
 * card that must not be interrupted, and the honest interface is to
 * finish the swap and let the user undo it afterwards.
 */
static int on_progress(int done, int total, void *ctx)
{
    (void)ctx;

    if (s_tick)
        s_tick(s_tick_job, done, total, s_tick_ctx);

    return 1;
}

/* ------------------------------------------------------------------ */
/* Paths                                                              */
/* ------------------------------------------------------------------ */

/** Build "<device>/<rel>". Returns 0 if the device is not usable. */
static int dev_path(atlas_device_id_t dev, const char *rel,
                    char *out, int size)
{
    return atlas_device_path(dev, rel, out, size) == ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* Inspection                                                          */
/* ------------------------------------------------------------------ */

int atlas_install_is_installed(atlas_device_id_t dev)
{
    char path[ATLAS_INSTALL_PATH_MAX];

    /*
     * Both halves have to be there. A marker with no BOOT.ELF is a card
     * whose boot program was deleted, and offering "update" for that
     * would copy a program onto a card that cannot start it.
     */
    if (!dev_path(dev, P_MARKER, path, sizeof(path))
        || !atlas_file_exists(path))
        return 0;

    if (!dev_path(dev, P_BOOT, path, sizeof(path)))
        return 0;

    return atlas_file_exists(path);
}

int atlas_install_has_backup(atlas_device_id_t dev)
{
    char path[ATLAS_INSTALL_PATH_MAX];

    if (!dev_path(dev, P_BACKUP_ELF, path, sizeof(path)))
        return 0;

    return atlas_file_exists(path);
}

atlas_err_t atlas_install_find_source(char *out, int size)
{
    unsigned int d, r;

    if (!out || size <= 0)
        return ATLAS_EINVAL;

    out[0] = '\0';

    for (d = 0; d < sizeof(s_source_dev) / sizeof(s_source_dev[0]); d++) {
        for (r = 0; r < sizeof(s_source_rel) / sizeof(s_source_rel[0]); r++) {
            if (!dev_path(s_source_dev[d], s_source_rel[r], out, size))
                continue;

            if (atlas_file_exists(out))
                return ATLAS_OK;
        }
    }

    out[0] = '\0';
    return ATLAS_ENOENT;
}

void atlas_install_console_id(char *out, int size)
{
    /* GetRomName writes exactly 14 characters and no terminator, so the
     * buffer is one larger and the terminator is placed by hand. */
    char rom[15];
    int i;

    if (!out || size <= 0)
        return;

    memset(rom, 0, sizeof(rom));
    GetRomName(rom);

    /* An unreadable ROM comes back blank. Reporting "?" is honest;
     * reporting an empty field looks like a drawing bug. */
    if (rom[0] == '\0') {
        snprintf(out, size, "?");
        return;
    }

    for (i = 0; i < 14 && i < size - 1; i++)
        out[i] = rom[i];

    out[i] = '\0';
}

int atlas_install_space_needed_kb(const atlas_install_job_t *job)
{
    int bytes;

    if (!job)
        return 0;

    switch (job->op) {
    case ATLAS_OP_INSTALL:
    case ATLAS_OP_UPDATE:
    case ATLAS_OP_REPAIR:
        break;

    default:
        /* Restore and uninstall move a file that is already on the card,
         * and backup copies one whose space is already accounted for. */
        return 0;
    }

    bytes = atlas_file_size(job->source);
    if (bytes <= 0)
        return INSTALL_MIN_FREE_KB;

    /* Doubled: the staged copy and the live one both exist for the few
     * seconds between the copy and the rename. */
    return (bytes * 2) / 1024 + 64;
}

int atlas_install_op_available(atlas_install_op_t op, atlas_device_id_t dev)
{
    if (!atlas_device_is_ready(dev))
        return 0;

    switch (op) {
    case ATLAS_OP_INSTALL:
        /* Installing over an existing installation is what Update is
         * for; offering both would let a user reinstall by a route that
         * skips the backup of their settings. */
        return !atlas_install_is_installed(dev);

    case ATLAS_OP_UPDATE:
    case ATLAS_OP_REPAIR:
    case ATLAS_OP_UNINSTALL:
        return atlas_install_is_installed(dev);

    case ATLAS_OP_BACKUP:
        return 1;

    case ATLAS_OP_RESTORE:
        return atlas_install_has_backup(dev);

    default:
        return 0;
    }
}

atlas_str_id_t atlas_install_step_label(atlas_install_step_t step)
{
    switch (step) {
    case ATLAS_STEP_CHECK:  return ATLAS_STR_INS_STEP_CHECK;
    case ATLAS_STEP_BACKUP: return ATLAS_STR_INS_STEP_BACKUP;
    case ATLAS_STEP_COPY:   return ATLAS_STR_INS_STEP_COPY;
    case ATLAS_STEP_CONFIG: return ATLAS_STR_INS_STEP_CONFIG;
    case ATLAS_STEP_VERIFY: return ATLAS_STR_INS_STEP_VERIFY;
    default:                return ATLAS_STR_INS_STEP_CHECK;
    }
}

/* ------------------------------------------------------------------ */
/* Job setup                                                           */
/* ------------------------------------------------------------------ */

/** Which steps this operation actually performs. */
static int step_applies(atlas_install_op_t op, atlas_install_step_t step)
{
    switch (op) {
    case ATLAS_OP_INSTALL:
        return 1;                       /* all five */

    case ATLAS_OP_UPDATE:
    case ATLAS_OP_REPAIR:
        /*
         * No config step: the settings on the card are the user's, and
         * an update that rewrote ATLAS.INI would silently discard the
         * video mode they need to see the screen at all.
         */
        return step != ATLAS_STEP_CONFIG;

    case ATLAS_OP_BACKUP:
        return step == ATLAS_STEP_CHECK || step == ATLAS_STEP_BACKUP;

    case ATLAS_OP_RESTORE:
    case ATLAS_OP_UNINSTALL:
        /* Copy moves the saved program back; verify then swaps it in. */
        return step == ATLAS_STEP_CHECK
            || step == ATLAS_STEP_COPY
            || step == ATLAS_STEP_VERIFY;

    default:
        return 0;
    }
}

/** Stop the job here, with a reason the user can act on. */
static void fail(atlas_install_job_t *job, atlas_err_t err,
                 atlas_str_id_t message)
{
    if (job->current >= 0 && job->current < ATLAS_STEP_COUNT)
        job->state[job->current] = ATLAS_STEP_FAILED;

    job->err = err;
    job->message = message;
    job->done = 1;
    job->current = ATLAS_STEP_COUNT;

    ATLAS_LOG("INS", "operation %d failed (%d)", (int)job->op, (int)err);
}

static atlas_str_id_t success_message(atlas_install_op_t op)
{
    switch (op) {
    case ATLAS_OP_INSTALL:   return ATLAS_STR_INS_OK_INSTALL;
    case ATLAS_OP_UPDATE:    return ATLAS_STR_INS_OK_UPDATE;
    case ATLAS_OP_REPAIR:    return ATLAS_STR_INS_OK_REPAIR;
    case ATLAS_OP_BACKUP:    return ATLAS_STR_INS_OK_BACKUP;
    case ATLAS_OP_RESTORE:   return ATLAS_STR_INS_OK_RESTORE;
    case ATLAS_OP_UNINSTALL: return ATLAS_STR_INS_OK_UNINSTALL;
    default:                 return ATLAS_STR_INS_OK_INSTALL;
    }
}

void atlas_install_begin(atlas_install_job_t *job, atlas_install_op_t op,
                         atlas_device_id_t target)
{
    int i;

    if (!job)
        return;

    memset(job, 0, sizeof(*job));

    job->op = op;
    job->target = target;
    job->err = ATLAS_OK;
    job->message = success_message(op);
    job->current = 0;

    for (i = 0; i < ATLAS_STEP_COUNT; i++)
        job->state[i] = step_applies(op, (atlas_install_step_t)i)
                      ? ATLAS_STEP_PENDING : ATLAS_STEP_SKIPPED;

    /*
     * Find the source now rather than at copy time. A job that cannot
     * possibly succeed should say so before the user watches four
     * checkmarks appear and then fail on the fifth.
     */
    if (op == ATLAS_OP_INSTALL || op == ATLAS_OP_UPDATE
        || op == ATLAS_OP_REPAIR) {
        if (atlas_install_find_source(job->source,
                                      sizeof(job->source)) != ATLAS_OK) {
            fail(job, ATLAS_ENOENT, ATLAS_STR_INS_E_NOSRC);
            return;
        }
    }

    if (!atlas_device_is_ready(target))
        fail(job, ATLAS_ENODEV, ATLAS_STR_INS_E_NOCARD);
}

/* ------------------------------------------------------------------ */
/* Steps                                                               */
/* ------------------------------------------------------------------ */

/**
 * Everything that can be checked before anything is written.
 *
 * Creating the folder tree happens here too: mkdir on a path that
 * already exists is the normal case, and doing it up front means the
 * copy step never fails for a missing parent - which on a card that
 * already has other homebrew on it would be a confusing way to fail.
 */
static atlas_err_t step_check(atlas_install_job_t *job)
{
    const atlas_device_t *d = atlas_device_get(job->target);
    char path[ATLAS_INSTALL_PATH_MAX];
    int need;

    if (!d || d->state != ATLAS_DEV_READY)
        return ATLAS_ENODEV;

    need = atlas_install_space_needed_kb(job);

    /*
     * free_kb is -1 on a device that cannot report it. That is not a
     * reason to refuse: the check exists to give a clear message
     * instead of a failed write halfway through, and a device with no
     * figure to check simply fails later if it is full.
     */
    if (need > 0 && d->free_kb >= 0 && d->free_kb < need) {
        ATLAS_LOG("INS", "need %d KB, card has %d KB", need, d->free_kb);
        return ATLAS_ENOSPC;
    }

    if (job->op == ATLAS_OP_RESTORE || job->op == ATLAS_OP_UNINSTALL) {
        if (!dev_path(job->target, P_BACKUP_ELF, path, sizeof(path))
            || !atlas_file_exists(path))
            return ATLAS_ENOENT;
    }

    if (dev_path(job->target, P_BOOT_DIR, path, sizeof(path)))
        atlas_file_mkdir_p(path);

    /*
     * The rest of the tree is created only where it belongs. An
     * uninstall must not leave behind the folders it is removing, and a
     * restore is not an AtlasPS2 operation at all.
     */
    if (job->op == ATLAS_OP_INSTALL || job->op == ATLAS_OP_UPDATE
        || job->op == ATLAS_OP_REPAIR || job->op == ATLAS_OP_BACKUP) {
        if (dev_path(job->target, P_BACKUP_DIR, path, sizeof(path)))
            atlas_file_mkdir_p(path);
    }

    if (job->op == ATLAS_OP_INSTALL) {
        if (dev_path(job->target, P_CFG_DIR, path, sizeof(path)))
            atlas_file_mkdir_p(path);
        if (dev_path(job->target, P_APPS_DIR, path, sizeof(path)))
            atlas_file_mkdir_p(path);
        if (dev_path(job->target, P_LANG_DIR, path, sizeof(path)))
            atlas_file_mkdir_p(path);
        if (dev_path(job->target, P_THEME_DIR, path, sizeof(path)))
            atlas_file_mkdir_p(path);
    }

    return ATLAS_OK;
}

/**
 * Preserve whatever is booting this console today.
 *
 * Written once and then left alone forever. After the first update the
 * rollback slot holds one of our own builds, so a backup that refreshed
 * itself would end up holding AtlasPS2 - and an uninstall restoring it
 * would "remove" AtlasPS2 by installing it again.
 *
 * A card with no BOOT.ELF at all is the normal first-install case on a
 * freshly formatted card: there is nothing to preserve, and that is
 * success, not failure.
 */
static atlas_err_t step_backup(atlas_install_job_t *job)
{
    char boot[ATLAS_INSTALL_PATH_MAX];
    char dest[ATLAS_INSTALL_PATH_MAX];
    char mark[ATLAS_INSTALL_PATH_MAX];
    atlas_err_t err;

    if (!dev_path(job->target, P_BOOT, boot, sizeof(boot))
        || !dev_path(job->target, P_BACKUP_ELF, dest, sizeof(dest)))
        return ATLAS_EINVAL;

    if (!atlas_file_exists(boot)) {
        ATLAS_LOG("INS", "no existing BOOT.ELF; nothing to preserve");
        return ATLAS_OK;
    }

    /*
     * Already have one? Keep it. This is the guarantee that makes
     * uninstall mean something.
     */
    if (atlas_file_exists(dest)) {
        ATLAS_LOG("INS", "original backup already present, kept");
        return ATLAS_OK;
    }

    /*
     * Do not archive our own program as if it were the user's previous
     * one. This happens when a card was installed by an older build
     * that never wrote a backup: copying the current BOOT.ELF would
     * make uninstall a no-op that looks like it worked.
     */
    if (atlas_install_is_installed(job->target)) {
        ATLAS_LOG("INS", "existing BOOT.ELF is AtlasPS2, not archived");
        return ATLAS_OK;
    }

    err = atlas_file_copy_verified(boot, dest, on_progress, NULL);
    if (err != ATLAS_OK)
        return err;

    /*
     * A note beside it, so a user who finds this folder in a file
     * manager knows what the file is and does not delete it as junk.
     */
    if (dev_path(job->target, P_BACKUP_MARK, mark, sizeof(mark))) {
        static const char note[] =
            "This is the BOOT.ELF that was on this Memory Card before\n"
            "AtlasPS2 was installed. Uninstalling AtlasPS2 puts it back.\n"
            "Deleting this folder removes that possibility.\n";

        atlas_file_write_atomic(mark, note, (int)sizeof(note) - 1);
    }

    return ATLAS_OK;
}

/**
 * Stage the new program under BOOT.NEW.
 *
 * The live BOOT.ELF is not touched. Until the verify step renames it,
 * this card boots exactly what it booted before - which is what makes a
 * power cut during the copy a non-event.
 */
static atlas_err_t step_copy(atlas_install_job_t *job)
{
    char dst[ATLAS_INSTALL_PATH_MAX];
    const char *src;
    char backup[ATLAS_INSTALL_PATH_MAX];

    if (!dev_path(job->target, P_BOOT_NEW, dst, sizeof(dst)))
        return ATLAS_EINVAL;

    if (job->op == ATLAS_OP_RESTORE || job->op == ATLAS_OP_UNINSTALL) {
        if (!dev_path(job->target, P_BACKUP_ELF, backup, sizeof(backup)))
            return ATLAS_EINVAL;
        src = backup;
    } else {
        src = job->source;
    }

    /* A leftover from an interrupted run is not evidence of anything;
     * it is a partial file that would fail verification anyway. */
    atlas_file_remove(dst);

    return atlas_file_copy_verified(src, dst, on_progress, NULL);
}

/**
 * The settings and the marker.
 *
 * Only ever runs for a fresh install, and only writes ATLAS.INI when
 * there is not one already: a card being reinstalled after a problem
 * still holds settings its owner chose, and overwriting them would make
 * "install" quietly destructive.
 */
static atlas_err_t step_config(atlas_install_job_t *job)
{
    char path[ATLAS_INSTALL_PATH_MAX];
    atlas_config_t cfg;
    char text[ATLAS_CFG_FILE_MAX];
    int len;

    if (!dev_path(job->target, P_CFG_DIR "/ATLAS.INI", path, sizeof(path)))
        return ATLAS_EINVAL;

    if (!atlas_file_exists(path)) {
        atlas_config_defaults(&cfg);

        len = atlas_config_format(&cfg, text, (int)sizeof(text));
        if (len > 0) {
            atlas_err_t err = atlas_file_write_atomic(path, text, len);

            /*
             * A card that took the program but not the settings still
             * boots: AtlasPS2 falls back to defaults and writes them
             * itself on the first save. Failing the whole install here
             * would roll back a working copy over a recoverable
             * problem.
             */
            if (err != ATLAS_OK)
                ATLAS_LOG("INS", "settings not written (%d)", (int)err);
        }
    }

    return ATLAS_OK;
}

/**
 * Verify what was staged, then make it live.
 *
 * The checksum was already compared during the copy, so this pass is
 * not redundant: it reads the file back after the intervening writes
 * and the device sync, which is the point at which a card with a
 * failing sector stops agreeing with itself.
 *
 * The swap is: current -> BOOT.BAK, BOOT.NEW -> BOOT.ELF. If the second
 * rename fails, the backup is renamed straight back, so the card is
 * never left without a boot program.
 */
static atlas_err_t step_verify(atlas_install_job_t *job)
{
    char live[ATLAS_INSTALL_PATH_MAX];
    char staged[ATLAS_INSTALL_PATH_MAX];
    char rollback[ATLAS_INSTALL_PATH_MAX];
    char marker[ATLAS_INSTALL_PATH_MAX];
    char source[ATLAS_INSTALL_PATH_MAX];
    const char *origin;
    u32 crc_new = 0, crc_src = 0;
    atlas_err_t err;

    if (!dev_path(job->target, P_BOOT, live, sizeof(live))
        || !dev_path(job->target, P_BOOT_NEW, staged, sizeof(staged))
        || !dev_path(job->target, P_BOOT_BAK, rollback, sizeof(rollback)))
        return ATLAS_EINVAL;

    if (job->op == ATLAS_OP_RESTORE || job->op == ATLAS_OP_UNINSTALL) {
        if (!dev_path(job->target, P_BACKUP_ELF, source, sizeof(source)))
            return ATLAS_EINVAL;
        origin = source;
    } else {
        origin = job->source;
    }

    err = atlas_file_crc32(staged, &crc_new, on_progress, NULL);
    if (err != ATLAS_OK)
        return err;

    err = atlas_file_crc32(origin, &crc_src, on_progress, NULL);
    if (err != ATLAS_OK)
        return err;

    if (crc_new != crc_src) {
        /* Nothing has been swapped yet, so removing the staged file
         * puts the card back exactly as it was. */
        ATLAS_LOG("INS", "staged copy mismatched (%08x != %08x)",
                  (unsigned)crc_new, (unsigned)crc_src);
        atlas_file_remove(staged);
        return ATLAS_EFORMAT;
    }

    /*
     * Rotate. The old rollback slot goes first because some Memory Card
     * drivers refuse to rename onto a name that exists.
     */
    atlas_file_remove(rollback);

    if (atlas_file_exists(live))
        atlas_file_rename(live, rollback);

    if (atlas_file_rename(staged, live) != ATLAS_OK) {
        /*
         * The swap failed with the live file already moved aside. Put
         * it straight back - this is the rollback the spec asks for,
         * and it is why the old file was renamed rather than deleted.
         */
        ATLAS_LOG("INS", "activation failed, rolling back");

        if (atlas_file_exists(rollback))
            atlas_file_rename(rollback, live);

        atlas_file_remove(staged);
        return ATLAS_EIO;
    }

    /*
     * The marker follows the program, not the other way round: it is
     * written only once the card actually boots AtlasPS2, and removed
     * as soon as it does not. Otherwise a failed uninstall would leave
     * a card claiming an installation it no longer has.
     */
    if (dev_path(job->target, P_MARKER, marker, sizeof(marker))) {
        if (job->op == ATLAS_OP_UNINSTALL || job->op == ATLAS_OP_RESTORE) {
            atlas_file_remove(marker);
        } else {
            char note[128];
            int n = snprintf(note, sizeof(note),
                             "%s %s\n", ATLAS_NAME, ATLAS_VERSION_STRING);

            atlas_file_write_atomic(marker, note, n);
        }
    }

    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* The pump                                                            */
/* ------------------------------------------------------------------ */

/** Map a step's failure onto something worth reading. */
static atlas_str_id_t message_for(atlas_install_step_t step, atlas_err_t err)
{
    switch (err) {
    case ATLAS_ENOSPC:
        return ATLAS_STR_INS_E_SPACE;

    case ATLAS_ENODEV:
        return ATLAS_STR_INS_E_NOCARD;

    case ATLAS_ENOENT:
        /* At check time this is a missing backup; anywhere else it is a
         * source that vanished under us. */
        return step == ATLAS_STEP_CHECK ? ATLAS_STR_INS_E_NOBACKUP
                                        : ATLAS_STR_INS_E_COPY;

    case ATLAS_EFORMAT:
        return ATLAS_STR_INS_E_VERIFY;

    case ATLAS_EIO:
        /* Only the activation swap reports EIO after moving anything,
         * and it always puts the old program back before returning. */
        return step == ATLAS_STEP_VERIFY ? ATLAS_STR_INS_E_ROLLBACK
                                         : ATLAS_STR_INS_E_COPY;

    default:
        return ATLAS_STR_INS_E_OTHER;
    }
}

int atlas_install_pump(atlas_install_job_t *job)
{
    atlas_install_step_t step;
    atlas_err_t err;

    if (!job || job->done)
        return 0;

    /* Walk past the steps this operation does not perform. */
    while (job->current < ATLAS_STEP_COUNT
           && job->state[job->current] == ATLAS_STEP_SKIPPED)
        job->current++;

    if (job->current >= ATLAS_STEP_COUNT) {
        job->done = 1;
        job->err = ATLAS_OK;
        job->message = success_message(job->op);
        return 0;
    }

    step = (atlas_install_step_t)job->current;

    job->state[step] = ATLAS_STEP_RUNNING;
    s_tick_job = job;

    switch (step) {
    case ATLAS_STEP_CHECK:  err = step_check(job);  break;
    case ATLAS_STEP_BACKUP: err = step_backup(job); break;
    case ATLAS_STEP_COPY:   err = step_copy(job);   break;
    case ATLAS_STEP_CONFIG: err = step_config(job); break;
    case ATLAS_STEP_VERIFY: err = step_verify(job); break;
    default:                err = ATLAS_EINVAL;     break;
    }

    s_tick_job = NULL;

    if (err != ATLAS_OK) {
        fail(job, err, message_for(step, err));
        return 0;
    }

    job->state[step] = ATLAS_STEP_DONE;
    job->current++;

    return 1;
}
