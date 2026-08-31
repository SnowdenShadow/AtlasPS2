/*
 * AtlasPS2 - screens.h
 *
 * The concrete screens. Each is a single static instance: the set is
 * fixed at build time, so navigation cannot fail to allocate.
 */
#ifndef ATLAS_SCREENS_H
#define ATLAS_SCREENS_H

#include "atlas/screen.h"

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

/** System information: version, video mode, module status. */
atlas_screen_t *atlas_screen_sysinfo(void);

/** Power options. Only the ones that can be performed safely appear. */
atlas_screen_t *atlas_screen_power(void);

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
