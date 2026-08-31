/*
 * AtlasPS2 - installer/main.c
 * Entry point for ATLAS_INSTALLER.ELF.
 *
 * Deliberately close to the launcher's own boot sequence - same IOP
 * reset, same modules, same video defaults - because the two run on the
 * same hardware and any divergence would mean the installer proving
 * something about a console the launcher then behaves differently on.
 *
 * What is left out is as important as what is here: no configuration is
 * read, no theme is loaded, no language file is looked for. This
 * program runs before any of those exist on the card, and reading a
 * setting written by a version of AtlasPS2 that is not yet installed
 * would be reading a file this program is about to create.
 */
#include <stdio.h>

#include <gsKit.h>
#include <kernel.h>

#include "ins_screen.h"

#include "atlas/atlas.h"
#include "atlas/boot.h"
#include "atlas/device.h"
#include "atlas/font.h"
#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/log.h"
#include "atlas/screen.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

#include "ui/assets/font_ui.h"
#include "ui/assets/font_title.h"

#define RGB(r, g, b) GS_SETREG_RGBAQ((r), (g), (b), 0x80, 0x00)

#define COL_BG   RGB(0x12, 0x14, 0x18)
#define COL_WARN RGB(0xFF, 0xB0, 0x4A)

int main(int argc, char *argv[])
{
    atlas_boot_status_t status;
    atlas_video_cfg_t vcfg;
    atlas_font_t *font_ui = NULL;
    atlas_font_t *font_title = NULL;
    int i;

    (void)argc;
    (void)argv;

    if (atlas_boot_iop_init(&status) == ATLAS_EFATAL)
        return 1;

    if (status.pad)
        atlas_input_init();

    /*
     * Safe defaults, never anything stored. The installer has to be
     * readable on the television of a user whose console does not yet
     * boot AtlasPS2 at all, so it takes no chances with the mode.
     */
    atlas_video_cfg_defaults(&vcfg);

    if (atlas_video_init(&vcfg) != ATLAS_OK)
        return 1;

    font_ui = atlas_font_create(&atlas_font_ui);
    font_title = atlas_font_create(&atlas_font_title);

    if (!font_ui || !font_title) {
        /* Same signal as the launcher's: a flashing background is
         * distinguishable from a hang on black, and without a font
         * there is no way to say more than that. */
        for (i = 0; i < 300; i++) {
            atlas_video_frame_begin((i / 30) & 1 ? COL_WARN : COL_BG);
            atlas_video_frame_end();
        }
        return 1;
    }

    atlas_device_init(status.memcard, status.usb);

    /*
     * Sweep every device before the first screen draws. One poll touches
     * one device, and the menu's whole top half is the detection report
     * - showing "no card" for a second because the sweep had not
     * finished would send a user to check a slot that is fine.
     */
    for (i = 0; i < ATLAS_DEV_COUNT * 2; i++)
        atlas_device_poll();

    ATLAS_LOG("INS", "%s installer %s", ATLAS_NAME, ATLAS_VERSION_STRING);

    atlas_ui_set_fonts(font_ui, font_title);
    atlas_screen_reset(atlas_ins_screen_menu());
    atlas_screen_run();

    atlas_device_shutdown();
    atlas_font_destroy(font_title);
    atlas_font_destroy(font_ui);
    atlas_input_shutdown();
    atlas_video_shutdown();

    return 0;
}
