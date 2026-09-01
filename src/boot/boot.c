/*
 * AtlasPS2 - boot.c
 * IOP reset and IRX module loading.
 */
#include <string.h>

#include <kernel.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

#include "atlas/boot.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Embedded IRX modules                                                */
/*                                                                     */
/* Generated at build time by $(PS2SDK)/bin/bin2c from the modules     */
/* shipped with ps2sdk. See the IRX_FILES list in the Makefile.        */
/* ------------------------------------------------------------------ */

#define ATLAS_IRX(name)                                     \
    extern unsigned char name##_irx[] __attribute__((aligned(16))); \
    extern unsigned int size_##name##_irx

ATLAS_IRX(iomanX);
ATLAS_IRX(fileXio);
ATLAS_IRX(sio2man);
ATLAS_IRX(padman);
ATLAS_IRX(mcman);
ATLAS_IRX(mcserv);
ATLAS_IRX(usbd);
ATLAS_IRX(bdm);
ATLAS_IRX(bdmfs_fatfs);
ATLAS_IRX(usbmass_bd);
ATLAS_IRX(ps2dev9);
ATLAS_IRX(ps2atad);
ATLAS_IRX(ps2hdd);
ATLAS_IRX(ps2fs);
ATLAS_IRX(poweroff);

#undef ATLAS_IRX

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static atlas_boot_status_t s_status;
static int s_usb_loaded;

/**
 * Load one embedded module.
 *
 * SifExecModuleBuffer copies the image from EE RAM into IOP RAM and
 * starts it. It needs sbv_patch_enable_lmb() to have run first, because
 * a stock IOP refuses to load modules that did not come from a device.
 *
 * @return ATLAS_OK on success. The module result code is also checked:
 *         a module can load and still refuse to start (mod_res < 0).
 */
static atlas_err_t load_module(const char *name, void *image, unsigned int size,
                               int arg_len, const char *args)
{
    int mod_res = 0;
    int id;

    id = SifExecModuleBuffer(image, size, arg_len, args, &mod_res);
    if (id < 0) {
        ATLAS_LOG("BOOT", "%s: load failed (%d)", name, id);
        return ATLAS_EFAIL;
    }

    if (mod_res != 0 && mod_res != 1) {
        /* 0 == RESIDENT, 1 == NO_RESIDENT_END: both are fine. */
        ATLAS_LOG("BOOT", "%s: start refused (%d)", name, mod_res);
        return ATLAS_EFAIL;
    }

    ATLAS_LOG("BOOT", "%s loaded (id %d)", name, id);
    return ATLAS_OK;
}

#define LOAD(name, arglen, args) \
    load_module(#name, name##_irx, size_##name##_irx, (arglen), (args))

/* ------------------------------------------------------------------ */
/* IOP reset                                                           */
/* ------------------------------------------------------------------ */

static atlas_err_t reset_iop(void)
{
    /*
     * RPC has to be up before we can ask the IOP to do anything, and it
     * has to be torn down again before the reset: SifExitRpc() releases
     * the RPC channels the IOP is about to lose anyway, and leaving
     * them registered across a reboot is what makes the SIF come back
     * in a state where the first SifExecModuleBuffer() never answers.
     */
    SifInitRpc(0);
    SifExitRpc();

    /*
     * SifIopReset returns 0 while the request could not be delivered,
     * which is a transient condition rather than a verdict, so this
     * retries until it lands. It is not bounded for the same reason the
     * sync below is not: there is no useful fallback from an IOP that
     * will not reset, and a bound short enough to expire on working
     * hardware turns a slow console into a black screen.
     */
    while (!SifIopReset("", 0))
        ;

    /*
     * Wait for the IOP to finish rebooting, and wait without a bound.
     *
     * This loop used to give up after 100000 iterations, which reads
     * like patience and is not: the body is two instructions, so the
     * whole budget expires in a few milliseconds. A real IOP takes
     * hundreds of milliseconds to come back. Under an emulator the
     * reset completes at once and the bound is never approached, which
     * is exactly why the fault only appeared on hardware - and appeared
     * as a black screen, because giving up here returns EFATAL and
     * main() has nothing to draw with yet.
     *
     * SifIopSync() is the only signal that the IOP is up, and there is
     * nothing useful to do without one. A console whose IOP never
     * returns is broken in a way no fallback reaches.
     */
    while (!SifIopSync())
        ;

    /* The reset tore down RPC with it, so bring it back. */
    SifInitRpc(0);
    SifLoadFileInit();

    /*
     * enable_lmb       : allow SifExecModuleBuffer (load from EE memory)
     * disable_prefix_check: allow paths outside the official whitelist,
     *                    which is what lets us open mass:/ and mc0:/
     *                    freely and is required by the ELF loader.
     */
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    ATLAS_LOG("BOOT", "IOP reset complete");
    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* Module groups                                                       */
/* ------------------------------------------------------------------ */

/*
 * iomanX replaces the stock ioman with the extended one, and fileXio is
 * the EE-side RPC client for it. Together they give one API (open/read/
 * dopen/dread) that works the same across mc0:, mass: and hdd0:, which
 * is what lets the device layer above stay device-agnostic.
 */
static atlas_err_t load_fileio(void)
{
    if (LOAD(iomanX, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(fileXio, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (fileXioInit() < 0) {
        ATLAS_LOG("BOOT", "fileXioInit failed");
        return ATLAS_EFAIL;
    }

    return ATLAS_OK;
}

static atlas_err_t load_pad(void)
{
    if (LOAD(sio2man, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(padman, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    return ATLAS_OK;
}

static atlas_err_t load_memcard(void)
{
    /* mcserv depends on mcman, so the order here is not arbitrary. */
    if (LOAD(mcman, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(mcserv, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    return ATLAS_OK;
}

atlas_err_t atlas_boot_load_usb(void)
{
    if (s_usb_loaded)
        return ATLAS_OK;

    /*
     * The modern stack, the same one OPL and wLaunchELF use:
     *
     *   usbd         - the USB host controller driver
     *   bdm          - Block Device Manager, the generic block layer
     *   usbmass_bd   - exposes a USB mass storage device to bdm
     *   bdmfs_fatfs  - FAT12/16/32 (and exFAT) filesystem on top of bdm
     *
     * The old usbhdfsd monolith still exists in ps2sdk but is no longer
     * the maintained path.
     */
    if (LOAD(usbd, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(bdm, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(bdmfs_fatfs, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(usbmass_bd, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    s_usb_loaded = 1;
    s_status.usb = 1;
    return ATLAS_OK;
}

/*
 * Each module depends on the one before it: ps2atad needs ps2dev9's
 * controller up, ps2hdd needs ps2atad to read sectors through, and
 * ps2fs needs ps2hdd's partition table to find a filesystem to mount.
 * Like the memory card group, failure here is never fatal - a console
 * with no HDD or no network adaptor simply never sees ATLAS_DEV_HDD
 * become ready.
 */
static atlas_err_t load_hdd(void)
{
    if (LOAD(ps2dev9, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(ps2atad, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(ps2hdd, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    if (LOAD(ps2fs, 0, NULL) != ATLAS_OK)
        return ATLAS_EFAIL;

    return ATLAS_OK;
}

static atlas_err_t load_poweroff(void)
{
    return LOAD(poweroff, 0, NULL);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_boot_iop_init(atlas_boot_status_t *status)
{
    atlas_err_t err;

    memset(&s_status, 0, sizeof(s_status));

    err = reset_iop();
    if (err != ATLAS_OK) {
        if (status)
            *status = s_status;
        return err;
    }
    s_status.iop_reset = 1;

    /*
     * From here on every failure is survivable. We record what worked
     * and let the UI tell the user which features are unavailable,
     * rather than refusing to boot.
     */
    if (load_fileio() == ATLAS_OK)
        s_status.fileio = 1;
    else
        ATLAS_LOG("BOOT", "file layer unavailable");

    if (load_pad() == ATLAS_OK)
        s_status.pad = 1;
    else
        ATLAS_LOG("BOOT", "pad modules unavailable");

    if (load_memcard() == ATLAS_OK)
        s_status.memcard = 1;
    else
        ATLAS_LOG("BOOT", "memory card modules unavailable");

    if (atlas_boot_load_usb() != ATLAS_OK)
        ATLAS_LOG("BOOT", "USB unavailable, continuing without it");

    if (load_hdd() == ATLAS_OK)
        s_status.hdd = 1;
    else
        ATLAS_LOG("BOOT", "HDD modules unavailable");

    if (load_poweroff() == ATLAS_OK)
        s_status.poweroff = 1;

    if (status)
        *status = s_status;

    /*
     * No file layer means no configuration, no application scanning and
     * no ELF launching. That is not a usable environment, but it is not
     * fatal either: Recovery can still draw a screen explaining it.
     */
    return s_status.fileio ? ATLAS_OK : ATLAS_EFAIL;
}

const atlas_boot_status_t *atlas_boot_status(void)
{
    return &s_status;
}
