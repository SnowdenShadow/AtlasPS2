/*
 * AtlasPS2 - device.h
 *
 * The unified storage layer.
 *
 * Every kind of storage the PS2 can reach - the two Memory Card slots,
 * USB mass storage, and later the internal HDD - is presented here as a
 * numbered slot with a mount path. Screens ask this module what exists
 * and get a path they can hand to fileXio; nothing above this file has
 * to know that a Memory Card is polled through libmc while a USB stick
 * appears as a mounted FAT volume.
 *
 * Detection is slow: probing a Memory Card slot with no card in it costs
 * milliseconds, and USB needs seconds to enumerate after the console
 * powers on. So the state here is a cache. atlas_device_poll() refreshes
 * it and must be called from the update half of the frame, never from
 * draw - a draw path that blocks turns a 60 Hz interface into a
 * stuttering one.
 *
 * The mount path is not always the block device name: the HDD mounts as
 * "pfs0:" (a PFS filesystem fileXioMount()ed on top of the "hdd0:"
 * block device), the same way mc0:/mass: are already filesystem-level
 * paths rather than raw device names.
 */
#ifndef ATLAS_DEVICE_H
#define ATLAS_DEVICE_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Storage kinds, in the order they are listed to the user. */
typedef enum {
    ATLAS_DEV_MC0 = 0,  /**< Memory Card slot 1            */
    ATLAS_DEV_MC1,      /**< Memory Card slot 2            */
    ATLAS_DEV_MASS,     /**< USB mass storage, first volume */
    ATLAS_DEV_HDD,      /**< Internal HDD, "__common" partition, read-only */
    ATLAS_DEV_COUNT
} atlas_device_id_t;

/** What the last poll found. */
typedef enum {
    ATLAS_DEV_ABSENT = 0,   /**< nothing in the slot / not connected     */
    ATLAS_DEV_READY,        /**< present, formatted, path is usable      */
    ATLAS_DEV_UNFORMATTED,  /**< present but carries no usable filesystem */
    ATLAS_DEV_ERROR         /**< present and failing; see `detail`       */
} atlas_device_state_t;

/**
 * A device slot. Read-only to callers; refreshed by atlas_device_poll().
 */
typedef struct {
    atlas_device_id_t    id;
    atlas_device_state_t state;

    /** Short label for the UI: "Memory Card 1", "USB". */
    const char          *name;

    /**
     * Mount path with a trailing colon, ready to have a path appended:
     * "mc0:", "mass:". Valid only while state is ATLAS_DEV_READY - the
     * string itself is static, but a device that is absent has nothing
     * behind the path.
     */
    const char          *path;

    /**
     * Free space in kilobytes, or -1 when unknown. Memory Cards report
     * it cheaply; a FAT volume would need a full cluster scan, which is
     * far too slow to run every poll, so USB reports -1.
     */
    int                  free_kb;

    /** Human-readable reason when state is ATLAS_DEV_ERROR, else NULL. */
    const char          *detail;
} atlas_device_t;

/**
 * Prepare the layer. Safe to call when the underlying modules failed to
 * load: those devices simply stay ATLAS_DEV_ABSENT forever.
 *
 * @param have_memcard  the mcman/mcserv modules loaded
 * @param have_usb      the usbd/bdm stack loaded
 * @param have_hdd      the ps2dev9/ps2atad/ps2hdd/ps2fs stack loaded
 */
atlas_err_t atlas_device_init(int have_memcard, int have_usb, int have_hdd);

/**
 * Refresh the cache. Call from update, not from draw.
 *
 * Each call probes at most one device, cycling through them, so the cost
 * is spread over frames instead of landing on one. A full sweep of four
 * devices therefore takes four calls.
 *
 * @return 1 if any device changed state since the previous poll, so a
 *         caller can rescan applications only when something actually
 *         moved rather than every frame.
 */
int atlas_device_poll(void);

/** The slot for `id`. Never NULL for a valid id; state may be ABSENT. */
const atlas_device_t *atlas_device_get(atlas_device_id_t id);

/** Convenience: is this device mounted and usable right now? */
int atlas_device_is_ready(atlas_device_id_t id);

/** Number of devices currently ATLAS_DEV_READY. */
int atlas_device_ready_count(void);

/**
 * Build a full path on a device: "mc0:/ATLAS/ATLAS.INI".
 *
 * Refuses rather than truncates. A silently shortened path can name a
 * different, existing file - and this layer is used by the file manager
 * and the installer, where writing to the wrong path destroys data.
 *
 * @return ATLAS_OK, ATLAS_EINVAL for a bad argument or a result that
 *         would not fit in `size`, or ATLAS_ENODEV if the device is not
 *         ready.
 */
atlas_err_t atlas_device_path(atlas_device_id_t id, const char *rel,
                              char *out, int size);

/** Release the layer. Devices become ABSENT. */
void atlas_device_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_DEVICE_H */
