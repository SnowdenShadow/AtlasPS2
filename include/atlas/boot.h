/*
 * AtlasPS2 - boot.h
 *
 * The IOP side of start-up: reset the IO processor, apply the SBV
 * patches that let us load modules from EE memory, then load the IRX
 * modules AtlasPS2 needs.
 *
 * WHY THE IOP IS RESET
 * -------------------
 * Whatever launched us (PS2BBL, OpenTuna, uLaunchELF, ...) left its own
 * modules resident on the IOP, and the IOP only has 2 MB. Resetting
 * gives us a clean, predictable module set instead of inheriting a
 * half-configured IOP whose usb or mc driver may be a different version
 * than the one we expect.
 *
 * The modules are EMBEDDED in our ELF rather than loaded from a device,
 * because at this point in the boot no filesystem is mounted yet - the
 * modules are precisely what makes filesystems work.
 */
#ifndef ATLAS_BOOT_H
#define ATLAS_BOOT_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Which optional module groups came up. */
typedef struct {
    int iop_reset;   /* IOP was reset and RPC re-established        */
    int fileio;      /* iomanX + fileXio: the unified file layer    */
    int pad;         /* sio2man + padman: controllers               */
    int memcard;     /* mcman + mcserv: memory cards                */
    int usb;         /* usbd + bdm + fatfs: USB mass storage        */
    int hdd;         /* ps2dev9 + ps2atad + ps2hdd + ps2fs: HDD     */
    int poweroff;    /* poweroff: software power-off support        */
} atlas_boot_status_t;

/**
 * Reset the IOP, apply SBV patches and load the module set.
 *
 * Every module group is optional except the file layer: a console with
 * no USB stick, or a broken USB port, must still reach the Home screen
 * from the memory card. Check the returned status to know what is
 * actually usable.
 *
 * @param status  filled in with what succeeded. May be NULL.
 * @return ATLAS_OK if the IOP came back and the file layer loaded,
 *         ATLAS_EFATAL if the IOP never came back (nothing can work).
 */
atlas_err_t atlas_boot_iop_init(atlas_boot_status_t *status);

/** Read-only view of what atlas_boot_iop_init() achieved. */
const atlas_boot_status_t *atlas_boot_status(void);

/**
 * Load the USB module stack on its own.
 *
 * Used when the user asks to retry USB from Settings after plugging a
 * drive in, without rebooting the whole console.
 */
atlas_err_t atlas_boot_load_usb(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_BOOT_H */
