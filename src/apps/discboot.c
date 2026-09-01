/*
 * AtlasPS2 - discboot.c
 * Starting a game from a disc image.
 *
 * The EE half of the drive emulation. Everything that can be checked
 * while the user is still looking at a screen happens in prepare();
 * run() is the handover, and past the IOP reset inside it there is
 * nothing left to report a failure with.
 *
 * WHAT THIS FILE DOES NOT DO
 * --------------------------
 * It does not read the image while the game runs, and it does not walk
 * a filesystem for the IOP module. The module does its own walk, once,
 * in its _start - see iop/atlascdvd/atlascdvd.h for why that is one
 * implementation of the sector arithmetic rather than two.
 *
 * What this file passes across is small: which device, which path,
 * which workarounds.
 */
#include <stdio.h>
#include <string.h>

#include <kernel.h>
#include <elf-loader.h>
#include <loadfile.h>
#include <malloc.h>
#include <sifrpc.h>
#include <iopcontrol.h>           /* SifIopSync()               */
#include <iopcontrol_special.h>   /* SifIopRebootBuffer()       */
#include <ioprpgen.h>             /* the IOPRP image it takes   */
#include <delaythread.h>          /* DelayThread()              */
#include <sbv_patches.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <io_common.h>

#include "atlas/btconf.h"
#include "atlas/discboot.h"
#include "atlas/device.h"
#include "atlas/image.h"
#include "atlas/input.h"
#include "atlas/log.h"
#include "atlas/video.h"

#include "../../iop/atlascdvd/atlascdvd.h"

/* ------------------------------------------------------------------ */
/* Embedded modules                                                    */
/*                                                                     */
/* The same mechanism boot.c uses, and for the same reason: after the  */
/* IOP reset there is no filesystem left to load a module from - these */
/* modules are what a filesystem is made of.                           */
/* ------------------------------------------------------------------ */

#define ATLAS_IRX(name)                                             \
    extern unsigned char name##_irx[] __attribute__((aligned(16))); \
    extern unsigned int size_##name##_irx

ATLAS_IRX(usbd);
ATLAS_IRX(bdm);
ATLAS_IRX(usbmass_bd);
ATLAS_IRX(atlascdvd);

#undef ATLAS_IRX

/*
 * The argument block, and why it is a file-scope object.
 *
 * The module reads it through a pointer this side writes as text. A
 * block on run()'s stack would be inside a frame that stops existing
 * the moment LoadELFFromFile jumps, and the module reads it during its
 * own _start - which happens after that jump. Static memory in our own
 * .bss is still ours until the game's ELF is loaded over us, and the
 * module copies the block before that can happen.
 */
static atlascdvd_arg_t s_arg __attribute__((aligned(64)));

/* ------------------------------------------------------------------ */
/* Preparing                                                           */
/* ------------------------------------------------------------------ */

/**
 * Split "mass:/DVD/GAME.ISO" into the device and the path within it.
 *
 * The module is told the device by index and the path without a prefix.
 * A path carrying "mass:" as well would be a second answer to the same
 * question, and two answers that can disagree is one too many.
 *
 * @return 0 on success.
 */
static int split_path(const char *full, int *out_index, char *out_path,
                      int out_size)
{
    const char *colon = strchr(full, ':');
    const char *rest;

    if (!colon)
        return -1;

    /*
     * Only mass: for now. bdm exposes USB and, in principle, other
     * block devices; the module reads any of them, but nothing else is
     * mounted at this point in the boot, and a device that is not there
     * fails inside the module where the user cannot see it.
     */
    if (strncmp(full, "mass", 4) != 0 || (colon - full) != 4)
        return -1;

    *out_index = 0;

    rest = colon + 1;

    /* Both separators, and a leading one either way: the module's
     * reader accepts both, and a path that starts without one is the
     * volume root just the same. */
    if (*rest != '/' && *rest != '\\') {
        if (snprintf(out_path, out_size, "/%s", rest) >= out_size)
            return -1;
    } else {
        if (snprintf(out_path, out_size, "%s", rest) >= out_size)
            return -1;
    }

    return 0;
}

/** atlas_disc_probe()'s read callback, over an open image. */
static int probe_read(void *ctx, u32 lba, u32 count, void *buf)
{
    return atlas_image_read(ctx, lba, count, buf);
}

atlas_err_t atlas_discboot_prepare(const char *path, atlas_discboot_t *out)
{
    atlas_image_t *img = 0;
    atlas_err_t err;
    const atlas_compat_t *entry;
    char within[ATLASCDVD_PATH_MAX];
    int index;

    if (!path || !path[0] || !out)
        return ATLAS_EINVAL;

    memset(out, 0, sizeof(*out));

    if (snprintf(out->path, sizeof(out->path), "%s", path)
        >= (int)sizeof(out->path)) {
        ATLAS_LOG("DISC", "path too long: %s", path);
        return ATLAS_EINVAL;
    }

    /*
     * The device has to be one the drive emulation can read from, and
     * that is decided here rather than in the module: a refusal here is
     * a message on screen, and a refusal there is a black one.
     */
    if (split_path(out->path, &index, within, sizeof(within)) != 0) {
        ATLAS_LOG("DISC", "%s is not on a supported device", path);
        return ATLAS_ENODEV;
    }

    out->device_index = index;

    /*
     * Identify the image through the ordinary file API, which is still
     * mounted and quiet. This is also the check that the file is a PS2
     * disc at all - a truncated download and a ZSO with a corrupt index
     * both fail here, on screen, rather than as a game that will not
     * start.
     */
    err = atlas_image_open(out->path, &img);
    if (err != ATLAS_OK) {
        ATLAS_LOG("DISC", "cannot open %s (%d)", out->path, (int)err);
        return err;
    }

    err = atlas_disc_probe(probe_read, img, &out->info);
    atlas_image_close(img);

    if (err != ATLAS_OK) {
        ATLAS_LOG("DISC", "%s is not a PS2 disc image (%d)",
                  out->path, (int)err);
        return err;
    }

    /*
     * A disc with no game ID is a legitimate thing to list and an
     * illegitimate thing to boot: there is no executable to enter and
     * no compatibility entry to key on.
     */
    if (out->info.id[0] == 0) {
        ATLAS_LOG("DISC", "%s has no boot entry", out->path);
        return ATLAS_EFORMAT;
    }

    entry = atlas_compat_find(out->info.id);
    if (entry) {
        out->compat     = *entry;
        out->has_compat = 1;
    } else {
        memset(&out->compat, 0, sizeof(out->compat));
        snprintf(out->compat.id, sizeof(out->compat.id), "%s",
                 out->info.id);
    }

    ATLAS_LOG("DISC", "%s: %s [%s] %s", out->path, out->info.id,
              atlas_disc_region_str(out->info.region),
              out->has_compat ? "compat entry found" : "no compat entry");

    return ATLAS_OK;
}

/* ------------------------------------------------------------------ */
/* Launching                                                           */
/* ------------------------------------------------------------------ */

static int load_module(const char *name, void *image, unsigned int size,
                       int arg_len, const char *args)
{
    int mod_res = 0;
    int id;

    id = SifExecModuleBuffer(image, size, arg_len, args, &mod_res);
    if (id < 0) {
        ATLAS_LOG("DISC", "%s: load failed (%d)", name, id);
        return -1;
    }

    if (mod_res != 0 && mod_res != 1) {
        ATLAS_LOG("DISC", "%s: start refused (%d)", name, mod_res);
        return -1;
    }

    return 0;
}

#define LOAD(name, arglen, args) \
    load_module(#name, name##_irx, size_##name##_irx, (arglen), (args))

/**
 * Build the block the module is started with.
 *
 * Kept in one function so that the fields and the reasons for them are
 * next to each other; getting one of these wrong is a game that boots
 * and misbehaves, which is the hardest kind of failure to attribute.
 */
static void fill_arg(const atlas_discboot_t *ready, const char *within)
{
    memset(&s_arg, 0, sizeof(s_arg));

    s_arg.magic        = ATLASCDVD_MAGIC;
    s_arg.version      = ATLASCDVD_ARG_VERSION;
    s_arg.device       = ATLASCDVD_DEV_BDM;
    s_arg.device_index = (unsigned int)ready->device_index;

    if (ready->compat.flags & ATLAS_COMPAT_FORCE_DVD)
        s_arg.flags |= ATLASCDVD_F_FORCE_DVD;

    if (ready->compat.flags & ATLAS_COMPAT_HIDE_TRAY)
        s_arg.flags |= ATLASCDVD_F_HIDE_TRAY;

    if (ready->compat.flags & ATLAS_COMPAT_SLOW_FIRST_READ)
        s_arg.flags |= ATLASCDVD_F_SLOW_FIRST;

    /*
     * The layer break. Zero for a single-layer image, which is what a
     * game asking about dual-layer information is told - and is the
     * truth for every image this reads today, since the dual-layer
     * field is not something an ISO carries. It is in the block so that
     * the module has somewhere to be told when it is.
     */
    s_arg.layer1_lba = 0;

    snprintf(s_arg.path, sizeof(s_arg.path), "%s", within);
}

/* ------------------------------------------------------------------ */
/* The boot list                                                       */
/* ------------------------------------------------------------------ */

/*
 * The filtered list and the IOPRP built around it.
 *
 * File-scope for the same reason s_arg is: the IOPRP is handed to
 * SifIopRebootBuffer, which DMAs it to the IOP, and the buffer must
 * still be there while that happens. Two kilobytes of .bss is cheaper
 * than reasoning about when it is safe to free.
 */
static atlas_btconf_t s_btconf;

/**
 * Read the console's own IOPBTCONF and take the drive modules out.
 *
 * The list differs between console revisions, so it is read rather than
 * written down: a list baked into this program would be a guess about
 * somebody else's machine, and a wrong guess is an IOP that boots
 * without a module it needed, after the last screen.
 *
 * fio rather than fileXio: this runs after atlas_device_shutdown(), and
 * rom0: is served by the IOP's own ROM driver, which is there from the
 * moment the IOP is alive and needs nothing of ours mounted.
 *
 * @return 0 on success.
 */
static int read_btconf(void)
{
    static char raw[ATLAS_BTCONF_MAX];
    atlas_err_t err;
    int fd, n;

    fd = fioOpen("rom0:IOPBTCONF", FIO_O_RDONLY);
    if (fd < 0) {
        ATLAS_LOG("DISC", "rom0:IOPBTCONF will not open (%d)", fd);
        return -1;
    }

    n = fioRead(fd, raw, (int)sizeof(raw));
    fioClose(fd);

    if (n <= 0) {
        ATLAS_LOG("DISC", "rom0:IOPBTCONF read %d", n);
        return -1;
    }

    /*
     * A file that filled the buffer exactly is one we may have read
     * only part of, and half a boot list is an IOP missing whichever
     * modules came after the cut. Refuse rather than guess.
     */
    if (n == (int)sizeof(raw)) {
        ATLAS_LOG("DISC", "rom0:IOPBTCONF larger than %d bytes",
                  (int)sizeof(raw));
        return -1;
    }

    err = atlas_btconf_filter(raw, n, &s_btconf);
    if (err != ATLAS_OK) {
        ATLAS_LOG("DISC", "IOPBTCONF not understood (%d)", (int)err);
        return -1;
    }

    ATLAS_LOG("DISC", "IOPBTCONF: %d bytes, %d drive modules removed",
              s_btconf.len, s_btconf.removed_count);

    return 0;
}

/**
 * Reset the IOP so that it comes up without the drive modules.
 *
 * SifIopRebootBuffer takes an IOPRP image - an archive of named files -
 * and the IOP boots from the module list in the IOPBTCONF it contains.
 * Ours is the console's own list with three lines removed, which is the
 * whole reason this path exists: a module cannot register a library
 * name that is already registered, so if the real cdvdman boots, ours
 * loads and is never called.
 *
 * @return 0 on success.
 */
static int reboot_without_drive(void)
{
    struct ioprpgen_ctx ctx;
    struct ioprpgen_memwrite_ctx mem;
    struct ioprpgen_entry entries[2];
    void *img;
    u32 size, again;

    memset(entries, 0, sizeof(entries));
    entries[0].m_name      = "IOPBTCONF";
    entries[0].m_data      = s_btconf.text;
    entries[0].m_data_size = (u32)s_btconf.len;
    /* entries[1] stays zeroed: the list is NULL-terminated. */

    /* Sized first, then filled: the library reports the length of the
     * image it would write when handed no buffer. */
    ioprpgen_setup_membuf(&ctx, &mem, NULL, 0);
    size = ioprpgen_write_ioprp(&ctx, entries);
    if (size == 0) {
        ATLAS_LOG("DISC", "IOPRP size refused");
        return -1;
    }

    img = memalign(64, size);
    if (!img) {
        ATLAS_LOG("DISC", "IOPRP allocation refused (%u bytes)",
                  (unsigned int)size);
        return -1;
    }

    ioprpgen_setup_membuf(&ctx, &mem, img, size);
    again = ioprpgen_write_ioprp(&ctx, entries);

    if (again != size) {
        ATLAS_LOG("DISC", "IOPRP wrote %u of %u",
                  (unsigned int)again, (unsigned int)size);
        free(img);
        return -1;
    }

    SifInitRpc(0);

    if (!SifIopRebootBuffer(img, (int)size)) {
        ATLAS_LOG("DISC", "IOP reboot refused");
        free(img);
        return -1;
    }

    while (!SifIopSync())
        ;

    /*
     * The buffer is not freed. SifIopRebootBuffer has already sent it,
     * but the heap it came from belongs to a program that is about to
     * be overwritten by the game's ELF, and a free() whose bookkeeping
     * touches memory after a reset is a risk taken for nothing.
     */

    SifInitRpc(0);
    return 0;
}

/**
 * Load the drive modules back, now that ours holds the cdvdman name.
 *
 * cdvdfsv is what the EE's own disc calls arrive through, and it
 * imports cdvdman - which is why it could not be in the boot list while
 * cdvdman was missing, and why it is loaded here instead. A game whose
 * EE code calls sceCdRead reaches our module through it.
 *
 * cdvdman itself is in `removed` and is deliberately not reloaded: it
 * is the module we replaced, and loading it now would find the name
 * taken and fail. It is skipped by name rather than by position, so a
 * revision that lists the modules in another order still works.
 */
static void restore_drive_modules(void)
{
    char path[8 + ATLAS_BTCONF_NAME_MAX];
    int i;

    for (i = 0; i < s_btconf.removed_count; i++) {
        const char *name = s_btconf.removed[i];

        if (strcmp(name, "CDVDMAN") == 0)
            continue;

        snprintf(path, sizeof(path), "rom0:%s", name);

        if (SifLoadModule(path, 0, NULL) < 0)
            ATLAS_LOG("DISC", "%s did not load", path);
    }
}

atlas_err_t atlas_discboot_run(const atlas_discboot_t *ready)
{
    char within[ATLASCDVD_PATH_MAX];
    char argbuf[16];
    char *args[1];
    int index, tries;

    if (!ready || !ready->path[0])
        return ATLAS_EINVAL;

    if (split_path(ready->path, &index, within, sizeof(within)) != 0)
        return ATLAS_ENODEV;

    if (ready->info.boot[0] == 0) {
        ATLAS_LOG("DISC", "no BOOT2 path");
        return ATLAS_EFORMAT;
    }

    /*
     * The boot list is read before anything is torn down, so that a
     * console whose IOPBTCONF this does not understand is a message on
     * screen rather than a black one. It is the last thing that can
     * still fail visibly.
     */
    if (read_btconf() != 0)
        return ATLAS_EFORMAT;

    fill_arg(ready, within);

    ATLAS_LOG("DISC", "launching %s (%s) via %s",
              ready->info.id, ready->info.boot, ready->path);

    /*
     * Past this line the console belongs to the game. Our subsystems go
     * down in the reverse of the order they came up, so it finds a
     * quiet machine rather than one with our DMA chains and interrupt
     * handlers still live.
     */
    atlas_device_shutdown();
    atlas_input_shutdown();
    atlas_video_shutdown();

    /*
     * The IOP reset, and why it happens here rather than in the loader.
     *
     * The game's own modules must be loaded onto an IOP that has our
     * drive emulation resident and no real cdvdman. LoadELFFromFile
     * resets the IOP itself, which would throw the module away; so the
     * reset happens now, the module is installed, and the game's ELF is
     * loaded afterwards with the reset already done.
     */
    SifInitRpc(0);

    /*
     * "rom0:UDNL rom0:EELOADCNF" is the reset that leaves the IOP
     * without its stock cdvdman: UDNL is the ROM updater, and the second
     * word names the module list it boots with - one that omits the
     * drive modules. A plain SifIopReset("") would bring the real
     * cdvdman back, and it would win the name: a module cannot register
     * a library that is already registered, so ours would load and never
     * be called.
     *
     * SifIopReset is the current SDK entry point for this. The mode
     * argument is 0; the encrypted-image variants in iopcontrol_special.h
     * take an IOPRP buffer, which is a different thing from a module
     * list and not what we have.
     */
    for (tries = 0; tries < 1000; tries++) {
        if (SifIopReset("rom0:UDNL rom0:EELOADCNF", 0))
            break;
    }

    if (tries >= 1000)
        return ATLAS_EFATAL;

    while (!SifIopSync())
        ;

    SifInitRpc(0);
    SifLoadFileInit();

    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    /*
     * The device stack the module reads through. No filesystem driver:
     * bdmfs_fatfs would mount the volume, and the module does not want
     * it mounted - it reads the FAT itself, from raw sectors, precisely
     * so that nothing walks a filesystem while the game runs.
     */
    if (LOAD(usbd, 0, NULL) != 0)
        return ATLAS_EFAIL;

    if (LOAD(bdm, 0, NULL) != 0)
        return ATLAS_EFAIL;

    if (LOAD(usbmass_bd, 0, NULL) != 0)
        return ATLAS_EFAIL;

    /*
     * USB enumeration is not instant, and the module refuses to stay
     * resident if the device is not there when it looks. Waiting a
     * fixed interval is crude, but the alternative - asking bdm - means
     * an RPC interface bdm does not export to the EE.
     *
     * Two seconds is longer than any stick observed and shorter than a
     * user decides the console has hung.
     */
    for (tries = 0; tries < 40; tries++)
        DelayThread(50000);

    /*
     * The module, and the block it reads. The address is passed as
     * text because that is what a module argument is: a string.
     */
    snprintf(argbuf, sizeof(argbuf), "%08x", (unsigned int)&s_arg);
    args[0] = argbuf;

    if (load_module("atlascdvd", atlascdvd_irx, size_atlascdvd_irx,
                    (int)strlen(argbuf) + 1, argbuf) != 0) {
        /*
         * The module refused. It does that when the file is not on the
         * device it was told, or is too fragmented to describe, or the
         * volume is not FAT - all things this side checked, so reaching
         * here means the device changed under us between then and now.
         *
         * There is no screen left to say so on. The caller is expected
         * to have warned the user that this is the point of no return.
         */
        ATLAS_LOG("DISC", "drive emulation refused to start");
        return ATLAS_EFAIL;
    }

    /*
     * The game's own executable, read through the module that has just
     * been installed. "cdrom0:" now means the image.
     */
    args[0] = (char *)ready->info.boot;
    LoadELFFromFile(ready->info.boot, 1, args);

    ATLAS_LOG("DISC", "loader refused %s", ready->info.boot);
    return ATLAS_EFORMAT;
}
