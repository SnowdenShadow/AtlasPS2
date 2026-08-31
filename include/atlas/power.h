/*
 * AtlasPS2 - power.h
 *
 * Leaving AtlasPS2: reboot, hand back to the console browser, or power
 * the console off.
 *
 * All of these tear the environment down first. The GS is released, the
 * pad ports are closed and the IOP is reset where the target needs it,
 * because whatever runs next expects to find the hardware in the state
 * it would be in after a cold boot - not sharing it with us.
 */
#ifndef ATLAS_POWER_H
#define ATLAS_POWER_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Whether the console can actually be powered off from software.
 *
 * Needs the poweroff IRX, which fails to load on some configurations.
 * The power menu hides the entry rather than offering an action that
 * would do nothing.
 */
int atlas_power_can_shutdown(void);

/** Power the console off. Does not return when it succeeds. */
void atlas_power_shutdown(void);

/**
 * Return to the console's own browser/OSD.
 *
 * This is the safe exit: it works on every console, needs nothing
 * installed, and is what the user wants when something has gone wrong.
 * Does not return when it succeeds.
 */
void atlas_power_exit_to_browser(void);

/**
 * Restart AtlasPS2 from the path it was launched from.
 *
 * @param self_path the ELF path, e.g. "mc0:/BOOT/BOOT.ELF". If NULL or
 *        unlaunchable this falls back to the browser rather than
 *        leaving the console on a black screen.
 */
void atlas_power_restart(const char *self_path);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_POWER_H */
