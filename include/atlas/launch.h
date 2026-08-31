/*
 * AtlasPS2 - launch.h
 *
 * Handing the console over to another program.
 *
 * This is the one operation AtlasPS2 cannot take back. Once the ELF is
 * running, our code is gone from memory - there is no return, no error
 * screen, and no way to recover except powering off. So everything that
 * can be checked is checked BEFORE the point of no return, and anything
 * that fails there fails while we are still on screen and able to say
 * why.
 */
#ifndef ATLAS_LAUNCH_H
#define ATLAS_LAUNCH_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check that `path` is worth handing the console to.
 *
 * Opens the file and reads its ELF header. A file that is not an ELF,
 * or is truncated, or is for another architecture, would otherwise
 * leave the user staring at a black screen with no way back - the exact
 * outcome this project exists to avoid.
 *
 * @return ATLAS_OK if it looks launchable,
 *         ATLAS_ENOENT if it cannot be opened,
 *         ATLAS_EFORMAT if it is not a PS2 ELF,
 *         ATLAS_EINVAL for a bad argument.
 */
atlas_err_t atlas_launch_check(const char *path);

/**
 * Launch the ELF at `path`. Does not return on success.
 *
 * Runs atlas_launch_check() first, then shuts our subsystems down in
 * the reverse of the order they were brought up, so the incoming
 * program finds a console in a known state rather than one with our
 * DMA chains and interrupt handlers still live.
 *
 * The IOP is deliberately NOT reset here. Most homebrew resets it
 * itself and expects to; the ones that do not rely on inheriting the
 * loaded modules, and resetting would take away the very drivers they
 * need. Either way the choice belongs to the program being launched.
 *
 * @param argc  arguments to pass, may be 0
 * @param argv  argument vector, may be NULL when argc is 0
 * @return only on failure: the same codes as atlas_launch_check().
 */
atlas_err_t atlas_launch_elf(const char *path, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_LAUNCH_H */
