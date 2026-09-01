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
 * Record the path AtlasPS2 was started from, for the restart entry.
 *
 * Called once from main() with argv[0]. What arrives there depends
 * entirely on the launcher: uLaunchELF passes a full path, some
 * bootstraps pass nothing at all, and a few pass a name with no device
 * prefix. Only a path carrying a device prefix is kept, because a bare
 * "BOOT.ELF" is not something LoadExecPS2() can resolve - it would fail
 * after the IOP had already been reset, at the one point in the program
 * where there is no screen left to report on.
 *
 * @param argv0 argv[0] as main() received it. NULL and "" are accepted.
 */
void atlas_power_set_self_path(const char *argv0);

/**
 * The stored self path, or NULL when there is none worth offering.
 *
 * The power menu uses this to decide whether the restart entry appears
 * at all: an entry that cannot work is worse than an absent one, since
 * it only fails after the console is already halfway through leaving.
 */
const char *atlas_power_self_path(void);

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
