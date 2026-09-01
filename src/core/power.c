/*
 * AtlasPS2 - power.c
 * Shutdown, reboot and handing the console back.
 */
#include <stdio.h>
#include <string.h>

#include <kernel.h>
#include <libpwroff.h>
#include <sifrpc.h>
#include <iopcontrol.h>

#include "atlas/power.h"
#include "atlas/boot.h"
#include "atlas/file.h"
#include "atlas/video.h"
#include "atlas/input.h"
#include "atlas/log.h"

static int s_poweroff_ready;

/* Empty means "no usable path", which is also the state before main()
 * has said anything - a launcher that passes no argv[0] then leaves the
 * restart entry hidden rather than offering a guess. */
static char s_self_path[128];

/**
 * Release everything AtlasPS2 owns.
 *
 * Order matters: the pad ports and the GS come down while the IOP is
 * still running the modules that back them. Tearing the IOP down first
 * would leave those calls talking to modules that no longer exist.
 */
static void teardown(void)
{
    atlas_input_shutdown();
    atlas_video_shutdown();
}

void atlas_power_set_self_path(const char *argv0)
{
    const char *colon;

    s_self_path[0] = '\0';

    if (!argv0 || !argv0[0])
        return;

    /*
     * A device prefix is the whole test. LoadExecPS2() has no working
     * directory to resolve a bare name against, so "BOOT.ELF" would
     * fail - and it would fail after SifIopReset(), with the GS already
     * released and nothing left that can draw an error. Requiring the
     * colon turns that into a restart entry that simply never appears.
     */
    colon = strchr(argv0, ':');
    if (!colon || colon == argv0)
        return;

    if ((int)strlen(argv0) >= (int)sizeof(s_self_path)) {
        ATLAS_LOG("POWER", "argv[0] too long to keep: %s", argv0);
        return;
    }

    /*
     * Checked once, here, rather than at the moment of restarting. By
     * then the check would be pointless: the fallback has to run
     * anyway, and the user has already committed to leaving.
     */
    if (!atlas_file_exists(argv0)) {
        ATLAS_LOG("POWER", "argv[0] does not resolve: %s", argv0);
        return;
    }

    snprintf(s_self_path, sizeof(s_self_path), "%s", argv0);
    ATLAS_LOG("POWER", "self path: %s", s_self_path);
}

const char *atlas_power_self_path(void)
{
    return s_self_path[0] ? s_self_path : NULL;
}

int atlas_power_can_shutdown(void)
{
    return atlas_boot_status()->poweroff;
}

void atlas_power_shutdown(void)
{
    if (!atlas_power_can_shutdown()) {
        ATLAS_LOG("POWER", "shutdown requested without the poweroff module");
        return;
    }

    teardown();

    /*
     * poweroffInit() starts the service thread the shutdown path needs.
     * It is done here rather than at boot because a console that never
     * opens the power menu has no use for the thread.
     */
    if (!s_poweroff_ready) {
        if (poweroffInit() < 0) {
            ATLAS_LOG("POWER", "poweroffInit failed");
            return;
        }
        s_poweroff_ready = 1;
    }

    poweroffShutdown();
}

void atlas_power_exit_to_browser(void)
{
    teardown();

    /*
     * The IOP is reset before handing over: the OSD expects the module
     * set a cold boot gives it, and ours is not that. Without this the
     * browser can come up with no memory card access.
     */
    SifIopReset("", 0);
    while (!SifIopSync())
        ;

    SifInitRpc(0);

    ExecOSD(0, NULL);
}

void atlas_power_restart(const char *self_path)
{
    if (!self_path) {
        atlas_power_exit_to_browser();
        return;
    }

    teardown();

    SifIopReset("", 0);
    while (!SifIopSync())
        ;

    SifInitRpc(0);

    LoadExecPS2(self_path, 0, NULL);

    /*
     * LoadExecPS2 is declared noreturn, so reaching here means the ELF
     * could not be started at all. Falling back to the browser leaves
     * the user somewhere usable instead of on a black screen.
     */
    atlas_power_exit_to_browser();
}
