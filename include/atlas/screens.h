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

/** System information: version, video mode, module status. */
atlas_screen_t *atlas_screen_sysinfo(void);

/** Power options. Only the ones that can be performed safely appear. */
atlas_screen_t *atlas_screen_power(void);

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
