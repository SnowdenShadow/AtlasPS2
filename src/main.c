/*
 * AtlasPS2 - main.c
 * Entry point and the top-level boot sequence.
 */
#include <stdio.h>

#include <gsKit.h>
#include <kernel.h>
#include <libpad.h>

#include "atlas/atlas.h"
#include "atlas/boot.h"
#include "atlas/video.h"
#include "atlas/input.h"
#include "atlas/font.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/screen.h"
#include "atlas/screens.h"
#include "atlas/log.h"

#include "ui/assets/font_ui.h"
#include "ui/assets/font_title.h"

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/*                                                                     */
/* Alpha is 0x80 everywhere: on the GS that value is 1.0, and anything */
/* above it over-saturates rather than getting brighter.               */
/* ------------------------------------------------------------------ */

#define RGB(r, g, b) GS_SETREG_RGBAQ((r), (g), (b), 0x80, 0x00)

#define COL_BG       RGB(0x12, 0x14, 0x18)
#define COL_TEXT     RGB(0xE8, 0xEA, 0xED)
#define COL_DIM      RGB(0x8A, 0x90, 0x99)
#define COL_ACCENT   RGB(0x4A, 0x9E, 0xFF)
#define COL_WARN     RGB(0xFF, 0xB0, 0x4A)

/* ------------------------------------------------------------------ */
/* Boot hotkeys                                                        */
/*                                                                     */
/* Read once, before the configuration is even parsed, so that a       */
/* configuration bad enough to break video or the UI can still be      */
/* escaped. Both are held while the console powers on.                 */
/* ------------------------------------------------------------------ */

#define HOTKEY_POLL_FRAMES 60 /* ~1 s: long enough to catch a hold that
                               * started before the pad was ready */

typedef struct {
    int recovery;   /* L1 + R1: minimal UI, no theme, no config      */
    int safe_video; /* R1 alone: force NTSC 4:3 with no user offsets */
} boot_hotkeys_t;

static boot_hotkeys_t read_hotkeys(void)
{
    boot_hotkeys_t keys = {0, 0};
    int i;

    /*
     * libpad needs a few frames to bring the port up, and the user is
     * holding the buttons the whole time, so we sample repeatedly and
     * latch anything we see rather than testing once and missing it.
     */
    for (i = 0; i < HOTKEY_POLL_FRAMES; i++) {
        u16 raw;

        atlas_input_update();
        raw = atlas_input_raw();

        if ((raw & PAD_L1) && (raw & PAD_R1))
            keys.recovery = 1;
        else if (raw & PAD_R1)
            keys.safe_video = 1;

        if (keys.recovery)
            break; /* recovery wins; no point sampling further */
    }

    /* Recovery implies safe video: it must come up on any TV. */
    if (keys.recovery)
        keys.safe_video = 1;

    return keys;
}

/* ------------------------------------------------------------------ */
/* Splash                                                              */
/* ------------------------------------------------------------------ */

static void draw_centered(atlas_font_t *font, float y, u64 color,
                          const char *text)
{
    float w = atlas_font_width(font, text);
    float x = atlas_video_safe_x()
            + (atlas_video_safe_w() - w) * 0.5f;

    atlas_font_draw(font, x, y, color, text);
}

/**
 * Boot splash: prove the whole stack works end to end - IOP modules,
 * GS, pad, font - and report what came up, so a console where (say) USB
 * failed shows why instead of silently lacking a device.
 *
 * Drawn before the theme and the screen stack exist, using the font
 * directly, so that a failure between here and the Home screen still
 * leaves something on screen.
 */
static void splash_loop(atlas_font_t *title, atlas_font_t *ui,
                        const atlas_boot_status_t *st,
                        const boot_hotkeys_t *keys)
{
    int line_h = atlas_font_line_height(ui);
    char buf[96];

    for (;;) {
        float y;

        atlas_input_update();

        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
            break;

        atlas_video_frame_begin(COL_BG);

        y = (float)atlas_video_safe_y();

        draw_centered(title, y + 40.0f, COL_TEXT, ATLAS_NAME);
        draw_centered(ui, y + 76.0f, COL_ACCENT, ATLAS_VERSION_STRING);

        y += 130.0f;

        snprintf(buf, sizeof(buf), "Video : %s", atlas_video_mode_name());
        atlas_font_draw(ui, (float)atlas_video_safe_x(), y, COL_DIM, buf);
        y += (float)line_h;

        snprintf(buf, sizeof(buf),
                 "Modules : file %s | pad %s | mc %s | usb %s",
                 st->fileio ? "ok" : "--",
                 st->pad ? "ok" : "--",
                 st->memcard ? "ok" : "--",
                 st->usb ? "ok" : "--");
        atlas_font_draw(ui, (float)atlas_video_safe_x(), y, COL_DIM, buf);
        y += (float)line_h;

        if (keys->recovery || keys->safe_video) {
            atlas_font_draw(ui, (float)atlas_video_safe_x(), y, COL_WARN,
                            keys->recovery ? "Recovery mode"
                                           : "Safe video mode");
            y += (float)line_h;
        }

        draw_centered(ui,
                      (float)(atlas_video_safe_y() + atlas_video_safe_h())
                          - (float)line_h * 2.0f,
                      COL_TEXT,
                      "Press X to continue");

        atlas_video_frame_end();
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    atlas_boot_status_t status;
    atlas_video_cfg_t vcfg;
    boot_hotkeys_t keys;
    atlas_font_t *font_ui = NULL;
    atlas_font_t *font_title = NULL;
    atlas_err_t err;

    (void)argc;
    (void)argv;

    /*
     * The IOP comes first: without it there is no pad to read hotkeys
     * from and no filesystem to read a configuration from. A fatal
     * result here means the IOP never came back, and there is nothing
     * useful left to do - not even draw an error, since we would have
     * to reset the IOP again to get anywhere.
     */
    err = atlas_boot_iop_init(&status);
    if (err == ATLAS_EFATAL)
        return 1;

    if (status.pad)
        atlas_input_init();

    keys = read_hotkeys();

    /*
     * Milestone 1 uses the safe defaults unconditionally; the stored
     * configuration is not read until the config module exists. The
     * hotkeys are already latched so the later milestones only have to
     * choose whether to apply what they read.
     */
    atlas_video_cfg_defaults(&vcfg);

    if (atlas_video_init(&vcfg) != ATLAS_OK)
        return 1;

    font_ui = atlas_font_create(&atlas_font_ui);
    font_title = atlas_font_create(&atlas_font_title);

    if (!font_ui || !font_title) {
        /*
         * No font means no way to explain the problem on screen. Flash
         * the background so the failure is at least visibly distinct
         * from a hang on black, then give up.
         */
        int i;
        for (i = 0; i < 300; i++) {
            atlas_video_frame_begin((i / 30) & 1 ? COL_WARN : COL_BG);
            atlas_video_frame_end();
        }
        return 1;
    }

    splash_loop(font_title, font_ui, &status, &keys);

    /*
     * From here the interface owns the frame loop. Recovery, when it
     * exists, will branch to its own root screen here instead of Home -
     * the hotkey is already latched.
     */
    atlas_ui_set_fonts(font_ui, font_title);
    atlas_screen_reset(atlas_screen_home());
    atlas_screen_run();

    atlas_font_destroy(font_title);
    atlas_font_destroy(font_ui);
    atlas_input_shutdown();
    atlas_video_shutdown();

    return 0;
}
