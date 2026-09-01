/*
 * AtlasPS2 - discboot.h
 *
 * Starting a game from a disc image.
 *
 * THE POINT OF NO RETURN, AGAIN
 * -----------------------------
 * launch.h describes handing the console to another ELF. This is the
 * same handover with one extra step: before the game's own executable
 * is loaded, a module is installed on the IOP that answers the game's
 * disc reads from a file. After that module is resident there is no
 * going back either - the IOP has been reset, our drivers are gone, and
 * the only ways out are the game booting or the power switch.
 *
 * So the order below is not an implementation detail, it is the whole
 * design:
 *
 *   1. identify the image and look up its compatibility entry
 *   2. reduce the file to a list of device sectors, on a filesystem
 *      that is still mounted and quiet
 *   3. reset the IOP and load the device drivers the module needs
 *   4. load the module, handing it that list
 *   5. set the video mode the game expects
 *   6. load the game's own executable through it
 *
 * Everything that can fail while the user is still looking at a screen
 * happens in steps 1 and 2. From step 3 on, a failure is a black
 * screen, which is why nothing after that point does anything that
 * could have been checked earlier.
 *
 * WHAT IS NOT VERIFIED
 * --------------------
 * Steps 1 and 2 are checked on the build machine (tests/test_disc.c,
 * tests/test_frag.c). Steps 3 to 6 cannot be: there is no host that
 * runs them and no emulator whose cdvdman is close enough to be
 * evidence. They are unverified until a console runs them, and that is
 * said here rather than left for the user to discover.
 */
#ifndef ATLAS_DISCBOOT_H
#define ATLAS_DISCBOOT_H

#include "atlas/atlas.h"
#include "atlas/compat.h"
#include "atlas/disc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * What was learned about an image before launching it.
 *
 * Filled by atlas_discboot_prepare(), which does every check that can
 * be made while the user can still be told about a failure.
 */
typedef struct {
    atlas_disc_info_t info;     /**< title, ID, region, size          */
    atlas_compat_t    compat;   /**< the entry that applies, if any    */
    int               has_compat;

    /** Absolute path as given, e.g. "mass:/DVD/GAME.ISO". */
    char path[256];

    /** Which bdm device the file is on, and its sector size. */
    int  device_index;
    u32  sector_size;

    /** How many runs the file is in. Kept for the UI to show: a file
     *  in 400 runs is one the user may want to defragment. */
    int  frag_count;
} atlas_discboot_t;

/**
 * Everything that can be checked while a failure is still visible.
 *
 * Identifies the image, looks up its compatibility entry, and reduces
 * the file to a list of device sectors. Does not touch the IOP, change
 * the video mode, or start anything.
 *
 * @return ATLAS_OK;
 *         ATLAS_ENOENT if the file is not there;
 *         ATLAS_EFORMAT if it is not a PS2 disc image;
 *         ATLAS_ENOMEM if it is too fragmented to describe
 *                      (ATLAS_FRAG_MAX runs);
 *         ATLAS_EIO if the device stopped responding;
 *         ATLAS_ENOTSUP if the image is on a device the drive
 *                      emulation cannot read from yet.
 */
atlas_err_t atlas_discboot_prepare(const char *path, atlas_discboot_t *out);

/**
 * Hand the console to the game. Does not return on success.
 *
 * Takes the result of atlas_discboot_prepare(), which must have
 * succeeded. Resets the IOP, loads the drivers and the drive emulation,
 * sets the video mode, and loads the game's executable.
 *
 * @return only on failure, and only from the part before the IOP is
 *         reset. After that a failure cannot be reported: there is
 *         nothing left to report with.
 */
atlas_err_t atlas_discboot_run(const atlas_discboot_t *ready);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_DISCBOOT_H */
