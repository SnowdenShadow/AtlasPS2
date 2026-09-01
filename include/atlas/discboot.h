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
 *   1. identify the image and look up its compatibility entry, on a
 *      filesystem that is still mounted and quiet
 *   2. reset the IOP and load the device drivers the module needs
 *   3. load the module, handing it the device and the path
 *   4. load the game's own executable through it
 *
 * Everything that can fail while the user is still looking at a screen
 * happens in step 1. From step 2 on, a failure is a black screen, which
 * is why nothing after that point does anything that could have been
 * checked earlier.
 *
 * The module works out for itself which sectors the file occupies: it
 * walks the FAT once, in its own _start, before the game exists. That
 * is why no extent list crosses this interface - see
 * iop/atlascdvd/atlascdvd.h for why there is one implementation of that
 * arithmetic rather than two.
 *
 * WHAT IS NOT VERIFIED
 * --------------------
 * Step 1 is checked on the build machine (tests/test_disc.c), as is the
 * sector arithmetic the module runs (tests/test_frag.c). Steps 2 to 4
 * cannot be: there is no host that runs them and no emulator whose
 * cdvdman is close enough to be evidence. They are unverified until a
 * console runs them, and that is said here rather than left for the
 * user to discover.
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

    /** Absolute path as given, e.g. "mass:/DVD/GAME.ISO". Unused when
     *  is_hdl is set - an HDL game has no path, only a partition. */
    char path[256];

    /** Which bdm device the file is on. Unused when is_hdl is set. */
    int  device_index;

    /**
     * Set by atlas_discboot_prepare_hdl() instead of
     * atlas_discboot_prepare(): the image is an HDL game partition on
     * the internal HDD, read by raw ATA transfer rather than through a
     * bdm device.
     */
    int is_hdl;

    /** Absolute ATA sector where ISO data begins. Valid when is_hdl. */
    u32 hdl_start_lba;

    /** Size of the ISO data in 512-byte sectors. Valid when is_hdl. */
    u32 hdl_total_sectors;
} atlas_discboot_t;

/**
 * Everything that can be checked while a failure is still visible.
 *
 * Identifies the image and looks up its compatibility entry. Does not
 * touch the IOP, change the video mode, or start anything.
 *
 * @return ATLAS_OK;
 *         ATLAS_ENOENT if the file is not there;
 *         ATLAS_EFORMAT if it is not a PS2 disc image, or carries no
 *                      boot entry to enter;
 *         ATLAS_EIO if the device stopped responding;
 *         ATLAS_ENODEV if the image is on a device the drive
 *                      emulation cannot read from yet - today that
 *                      means anything but mass:.
 */
atlas_err_t atlas_discboot_prepare(const char *path, atlas_discboot_t *out);

/**
 * Same as atlas_discboot_prepare(), for an HDL game partition on the
 * internal HDD instead of a file on a bdm device.
 *
 * @param start_lba      first ATA sector of the ISO data (game.c has
 *                        already skipped HDL's own 4 MB header).
 * @param total_sectors  size of the ISO data in 512-byte sectors.
 * @param display_name   shown in place of a path in the confirmation
 *                        and failure dialogs, since there is no path.
 */
atlas_err_t atlas_discboot_prepare_hdl(u32 start_lba, u32 total_sectors,
                                       const char *display_name,
                                       atlas_discboot_t *out);

/**
 * Hand the console to the game. Does not return on success.
 *
 * Takes the result of atlas_discboot_prepare(), which must have
 * succeeded. Resets the IOP, loads the drivers and the drive emulation,
 * and loads the game's executable.
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
