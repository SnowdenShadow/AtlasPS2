/*
 * AtlasPS2 - game.h
 *
 * Finding disc images on the attached devices.
 *
 * WHY THIS IS NOT app.h WITH A DIFFERENT SUFFIX
 * ---------------------------------------------
 * An application is anything with an ELF header, anywhere the scan
 * looks. A disc image is only usable if the drive emulation can read
 * it, and today that means one device: the USB stick. Listing an image
 * on a Memory Card would be listing something that cannot be started,
 * and a row that always refuses is worse than a row that is not there.
 *
 * So this scan is deliberately narrower than the application scan, and
 * the narrowness is the point rather than an omission.
 *
 * WHAT IT DOES NOT DO
 * -------------------
 * It does not open the images. Identifying one means reading its
 * volume descriptor and its SYSTEM.CNF, which on a USB stick costs
 * enough that doing it for thirty files would be a visible stall on
 * entering the screen. The title, the ID and the region are read for
 * one image, when the user chooses it, by atlas_discboot_prepare().
 *
 * What is listed is therefore the filename, which is what the user
 * named it, and that is honest: nothing here claims to know what a file
 * contains until something has read it.
 */
#ifndef ATLAS_GAME_H
#define ATLAS_GAME_H

#include "atlas/atlas.h"
#include "atlas/device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bounds. A stick holding more than 128 images is possible; one where
 * the 129th is the one you wanted is unlikely enough to be worth the
 * fixed array, which cannot fail partway through a scan.
 */
#define ATLAS_GAME_MAX       128
#define ATLAS_GAME_NAME_MAX  64
#define ATLAS_GAME_PATH_MAX  128

typedef struct {
    /** Filename without its extension, tidied for display. */
    char name[ATLAS_GAME_NAME_MAX];

    /** Full path, ready for atlas_discboot_prepare(). */
    char path[ATLAS_GAME_PATH_MAX];

    /** Which device it was found on. */
    atlas_device_id_t device;
} atlas_game_t;

/**
 * Rescan for disc images.
 *
 * Blocking, and visible on a slow stick. Call it on entering the screen
 * and on an explicit rescan, never per frame.
 *
 * @return the number of images found.
 */
int atlas_game_scan(void);

/** How many images the last scan found. */
int atlas_game_count(void);

/** Entry `i`, or NULL if out of range. */
const atlas_game_t *atlas_game_get(int i);

/** Whether a scan has ever run: "none here" and "not looked" differ. */
int atlas_game_scanned(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_GAME_H */
