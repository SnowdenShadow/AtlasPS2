/*
 * AtlasPS2 - launch.c
 * Handing the console over to another program.
 */
#include <string.h>
#include <stdio.h>

#include <kernel.h>
#include <elf-loader.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

#include "atlas/launch.h"
#include "atlas/video.h"
#include "atlas/input.h"
#include "atlas/device.h"
#include "atlas/log.h"

/*
 * WHY THE HEADER IS CHECKED HERE
 * ------------------------------
 * The SDK's loader validates the ELF magic with a `tne` - a trap
 * instruction. Handing it a file that is not an ELF does not return an
 * error, it raises an exception, and an exception at that point is a
 * black screen with no way back but the power switch. The file has to
 * be rejected while we are still on screen and able to say why.
 */

#define ELF_HEADER_MIN 52   /* size of a 32-bit ELF header */

/* e_machine: MIPS. The EE is a MIPS R5900 and every PS2 ELF says this. */
#define ELF_EM_MIPS 8

/* e_type: 2 = ET_EXEC. A relocatable object or a shared object is not
 * something the loader can run. */
#define ELF_ET_EXEC 2

atlas_err_t atlas_launch_check(const char *path)
{
    unsigned char hdr[ELF_HEADER_MIN];
    int fd, n;
    unsigned int e_type, e_machine;

    if (!path || !path[0])
        return ATLAS_EINVAL;

    fd = fileXioOpen(path, FIO_O_RDONLY);
    if (fd < 0) {
        ATLAS_LOG("RUN", "cannot open %s (%d)", path, fd);
        return ATLAS_ENOENT;
    }

    n = fileXioRead(fd, hdr, (int)sizeof(hdr));
    fileXioClose(fd);

    /* Short read: a truncated file, or a copy that was interrupted. */
    if (n < ELF_HEADER_MIN) {
        ATLAS_LOG("RUN", "%s too short (%d bytes)", path, n);
        return ATLAS_EFORMAT;
    }

    if (hdr[0] != 0x7F || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F') {
        ATLAS_LOG("RUN", "%s is not an ELF", path);
        return ATLAS_EFORMAT;
    }

    /*
     * Little-endian 32-bit, which is what the EE runs. A 64-bit or
     * big-endian file is somebody else's binary that happens to share
     * the extension.
     */
    if (hdr[4] != 1 || hdr[5] != 1) {
        ATLAS_LOG("RUN", "%s is not a 32-bit LE ELF", path);
        return ATLAS_EFORMAT;
    }

    /* Both fields are little-endian halfwords at fixed offsets. */
    e_type    = (unsigned int)hdr[16] | ((unsigned int)hdr[17] << 8);
    e_machine = (unsigned int)hdr[18] | ((unsigned int)hdr[19] << 8);

    if (e_type != ELF_ET_EXEC) {
        ATLAS_LOG("RUN", "%s is not executable (type %u)", path, e_type);
        return ATLAS_EFORMAT;
    }

    if (e_machine != ELF_EM_MIPS) {
        ATLAS_LOG("RUN", "%s is not MIPS (machine %u)", path, e_machine);
        return ATLAS_EFORMAT;
    }

    return ATLAS_OK;
}

atlas_err_t atlas_launch_elf(const char *path, int argc, char **argv)
{
    char argv0[256];
    char *args[1];
    atlas_err_t err;

    err = atlas_launch_check(path);
    if (err != ATLAS_OK)
        return err;

    ATLAS_LOG("RUN", "launching %s", path);

    /*
     * Past this line the console belongs to the other program. Shut our
     * subsystems down in the reverse of the order they came up, so it
     * finds a quiet machine rather than one with our DMA chains and
     * interrupt handlers still live: gsKit's handlers would fire into
     * freed memory the moment the new program touched the GS.
     */
    atlas_device_shutdown();
    atlas_input_shutdown();
    atlas_video_shutdown();

    /*
     * argv[0] is the program's own path. Homebrew that loads files
     * beside itself - a config, a theme, a plugin - derives their
     * location from it, and one launched with an empty argv[0] looks
     * for them on whatever device it guesses.
     */
    if (argc <= 0 || !argv) {
        snprintf(argv0, sizeof(argv0), "%s", path);
        args[0] = argv0;
        argc = 1;
        argv = args;
    }

    /*
     * LoadELFFromFile reads the whole file into EE RAM, THEN resets the
     * IOP, then jumps. The order matters: after the reset there is no
     * driver left to read the file with, so a loader that reset first
     * could never launch anything off USB.
     *
     * The reset is right for a launcher. Homebrew expects a clean IOP
     * and loads the modules it wants; leaving ours resident would give
     * it a second, conflicting copy of usbd or mcman.
     *
     * It needs sbv_patch_disable_prefix_check(), which boot.c already
     * applied at start-up.
     */
    LoadELFFromFile(path, argc, argv);

    /*
     * Only reached if the loader refused - a file that passed the
     * header check but has no loadable segments, say. Our subsystems
     * are already down, so there is nothing to draw with and nothing
     * to return to. The caller re-initialises or gives up.
     */
    ATLAS_LOG("RUN", "loader refused %s", path);
    return ATLAS_EFORMAT;
}
