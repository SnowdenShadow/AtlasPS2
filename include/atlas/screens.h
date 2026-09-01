/*
 * AtlasPS2 - screens.h
 *
 * The concrete screens. Each is a single static instance: the set is
 * fixed at build time, so navigation cannot fail to allocate.
 */
#ifndef ATLAS_SCREENS_H
#define ATLAS_SCREENS_H

#include "atlas/screen.h"
#include "atlas/install.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The root screen: the main menu and the device indicators. */
atlas_screen_t *atlas_screen_home(void);

/**
 * Storage: what each slot holds and, when it is unusable, why.
 *
 * Polls while open, so a USB stick that takes seconds to enumerate
 * appears without the user having to leave and come back.
 */
atlas_screen_t *atlas_screen_devices(void);

/**
 * The homebrew found on the attached devices, and launching it.
 *
 * Scans on entry and on an explicit rescan, never per frame: a walk
 * over a Memory Card's directories costs a visible fraction of a
 * second, and a screen that stutters while browsing is worse than one
 * that needs a button press to refresh.
 */
atlas_screen_t *atlas_screen_apps(void);

/**
 * The file manager: browse, launch, copy, move, rename, delete.
 *
 * Starts at the device list rather than inside one device, because
 * "go up from mc0:/" is a question the drivers answer differently.
 * Every operation that changes a file is behind the action menu and
 * asks first; anything atlas_fs_is_protected() names asks a second
 * time, with different words.
 */
atlas_screen_t *atlas_screen_files(void);

/** System information: version, video mode, module status. */
atlas_screen_t *atlas_screen_sysinfo(void);

/**
 * Video settings, applied live and confirmed before they are kept.
 *
 * The one screen whose settings can make the screen itself unreadable,
 * so a change to the mode or the aspect reverts on its own unless the
 * user confirms it from the new mode. Nothing is written to ATLAS.INI
 * until the Save entry is chosen.
 */
atlas_screen_t *atlas_screen_video(void);

/**
 * The theme picker, previewed live and confirmed by saving.
 *
 * Offers the built-in theme plus whatever is on the attached devices.
 * Highlighting a row applies it at once - a palette cannot make the
 * television lose the picture, so the guarded flow the video screen
 * needs would only be in the way here. Leaving without saving puts
 * back the theme that was live on entry.
 */
atlas_screen_t *atlas_screen_theme(void);

/**
 * Every setting, in one list, grouped by section.
 *
 * Mostly doors: video, theme, devices and applications each have a
 * screen that owns them, and this one opens those rather than keeping
 * a second copy of their controls - two copies of the video clamps is
 * one place for them to disagree, and the one that disagrees leaves a
 * television showing nothing.
 *
 * What it edits itself is what has no home elsewhere: the language,
 * the startup screen, the two cosmetic switches and the two boot keys.
 * Saving reads ATLAS.INI and replaces only those, so it never undoes a
 * video mode set from the screen it just opened.
 */
atlas_screen_t *atlas_screen_settings(void);

/** Power options. Only the ones that can be performed safely appear. */
atlas_screen_t *atlas_screen_power(void);

/**
 * The first-boot wizard: language, display, scan, then Home.
 *
 * A root, not a screen pushed over Home, because it runs before there
 * is a configuration to draw Home from and because there is nothing
 * sensible behind it to go back to. Used only when the configuration
 * load reported ATLAS_CFG_DEFAULTS - that is, no file was found at all,
 * which on a working install means this console has never run AtlasPS2.
 */
atlas_screen_t *atlas_screen_wizard(void);

/**
 * The recovery root, used instead of Home when L1+R1 was held at boot.
 *
 * Its own root rather than a screen pushed over Home: the whole point
 * of recovery is that nothing optional has been loaded, and a Home
 * screen underneath would be one drawn from a configuration that may be
 * exactly what is broken. Back from here does not return to a launcher
 * that was never started - it offers the browser instead.
 */
atlas_screen_t *atlas_screen_recovery(void);

/**
 * The five-step progress list for an install engine job.
 *
 * Shared with the installer ELF. The caller hands over a job already
 * set up by atlas_install_begin(), so one that cannot start shows its
 * reason here rather than flashing past.
 */
atlas_screen_t *atlas_screen_install_run(const atlas_install_job_t *job);

/**
 * A placeholder for a screen that is specified but not yet implemented.
 *
 * Milestones land one subsystem at a time, and a menu entry that opens
 * a black screen looks like a crash. This says what it will do and how
 * to go back, so a partially built release is still navigable.
 */
atlas_screen_t *atlas_screen_todo(const char *title, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_SCREENS_H */
