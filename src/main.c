/*
 * AtlasPS2 - main.c
 * Entry point and the top-level boot sequence.
 */
#include <stdio.h>
#include <string.h>

#include <gsKit.h>
#include <kernel.h>
#include <libpad.h>

#include "atlas/atlas.h"
#include "atlas/boot.h"
#include "atlas/config.h"
#include "atlas/i18n.h"
#include "atlas/video.h"
#include "atlas/input.h"
#include "atlas/device.h"
#include "atlas/fav.h"
#include "atlas/font.h"
#include "atlas/launch.h"
#include "atlas/power.h"
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
/* Settings                                                            */
/* ------------------------------------------------------------------ */

/**
 * Read ATLAS.INI and apply what it says.
 *
 * Called after the device layer is up and after something is already on
 * screen, so that everything here can fail without costing a picture.
 *
 * Two of the three settings are applied unconditionally. The video
 * settings are not: a mode this television cannot display leaves a user
 * with a black screen and a configuration they cannot reach to fix, so
 * holding R1 at boot skips them entirely. That is the escape hatch, and
 * it only works if it is honoured here rather than in the video module,
 * which has no idea a hotkey exists.
 */
static void load_settings(const boot_hotkeys_t *keys,
                          atlas_config_t *cfg,
                          atlas_config_origin_t *origin)
{
    atlas_video_cfg_t running;
    int i;

    /*
     * Recovery deliberately reads nothing: it exists for the case where
     * the stored configuration is what broke the console, and a
     * recovery mode that loads the file it is meant to repair is no
     * recovery at all.
     *
     * The defaults are still filled in, because the caller reads the
     * struct either way and a recovery boot must not hand it garbage.
     * The origin says LOADED rather than DEFAULTS so that skipping the
     * file is not mistaken for a first boot: recovery must reach its
     * own root, never the wizard.
     */
    if (keys->recovery) {
        ATLAS_LOG("CFG", "recovery: configuration not read");
        atlas_config_defaults(cfg);
        *origin = ATLAS_CFG_LOADED;
        return;
    }

    /*
     * One poll only touches one device, so a full sweep takes as many
     * calls as there are devices. Without this the first read would run
     * before any card had been looked at and always find nothing.
     */
    for (i = 0; i < ATLAS_DEV_COUNT; i++)
        atlas_device_poll();

    atlas_config_load(cfg, origin);

    atlas_i18n_set_lang(cfg->lang);
    atlas_i18n_load_overrides();

    /*
     * Before the video check, because a theme is not a video mode: R1
     * is held to escape a picture the television cannot show, and a
     * palette has nothing to do with that. Whether it succeeds is not
     * examined - a missing theme leaves the built-in one active, which
     * is a cosmetic disappointment and must never be a boot failure.
     */
    if (cfg->theme[0] != '\0' && strcmp(cfg->theme, "default") != 0)
        atlas_theme_load(cfg->theme);

    /*
     * Favorites are read here, in the one place a device sweep has
     * already happened. Recovery returned above without them, which is
     * correct: recovery lists nothing to launch.
     */
    atlas_fav_load();

    if (keys->safe_video) {
        ATLAS_LOG("CFG", "safe video: stored video settings skipped");
        return;
    }

    /*
     * Re-opening the screen with identical settings would still cost a
     * mode change and a visible flicker, so it is skipped when the file
     * asked for what is already running.
     */
    atlas_video_cfg_defaults(&running);

    if (memcmp(&cfg->video, &running, sizeof(running)) != 0)
        atlas_video_apply(&cfg->video);
}

/* ------------------------------------------------------------------ */
/* Auto-launch                                                         */
/*                                                                     */
/* [boot] default_app with a non-zero timeout launches that program    */
/* without the user touching anything. It is the one feature here that */
/* can take the console away from its own menu, so it is built to be   */
/* escapable: any button cancels, and the count is drawn the whole     */
/* time rather than running silently.                                  */
/* ------------------------------------------------------------------ */

#define AUTOBOOT_FPS 60

/**
 * Count down and launch, unless the user says otherwise.
 *
 * Deliberately drawn with the font directly rather than through the UI
 * layer: this runs before the screen stack is started, and giving it a
 * screen of its own would mean the stack had a root that vanishes,
 * which is the one shape atlas_screen_pop() has no answer for.
 *
 * A launch that fails simply returns, and the menu comes up as usual -
 * a bad path in the configuration must not be able to strand a console
 * short of its own interface.
 */
static void autoboot(atlas_font_t *ui, const atlas_config_t *cfg)
{
    int left;
    char buf[160];

    if (cfg->default_app[0] == '\0' || cfg->timeout <= 0)
        return;

    /*
     * Checked before the countdown, not after: a path that no longer
     * resolves - a USB stick left unplugged, an application deleted -
     * should cost the user nothing, not several seconds of watching a
     * timer run down to a failure.
     */
    if (atlas_launch_check(cfg->default_app) != ATLAS_OK) {
        ATLAS_LOG("BOOT", "default_app is not launchable: %s",
                  cfg->default_app);
        return;
    }

    for (left = cfg->timeout * AUTOBOOT_FPS; left > 0; left--) {
        float y;

        atlas_input_update();

        /*
         * Any button at all cancels, not a specific one. A user who
         * wants their menu is pressing something; making them find the
         * right key while a timer runs is the opposite of an escape.
         */
        if (atlas_input_pressed() != 0) {
            ATLAS_LOG("BOOT", "auto-launch cancelled");
            return;
        }

        atlas_video_frame_begin(COL_BG);

        y = (float)atlas_video_safe_y() + 60.0f;
        draw_centered(ui, y, COL_TEXT, cfg->default_app);

        snprintf(buf, sizeof(buf), "%d",
                 (left + AUTOBOOT_FPS - 1) / AUTOBOOT_FPS);
        draw_centered(ui, y + 40.0f, COL_ACCENT, buf);

        draw_centered(ui, y + 80.0f, COL_DIM,
                      "Press any button to stay in AtlasPS2");

        atlas_video_frame_end();
    }

    atlas_recent_note(cfg->default_app);
    atlas_fav_save();

    ATLAS_LOG("BOOT", "auto-launching %s", cfg->default_app);
    atlas_launch_elf(cfg->default_app, 0, NULL);

    /* Only reached when the launch failed. The menu follows. */
    ATLAS_LOG("BOOT", "auto-launch failed; continuing to the menu");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    atlas_boot_status_t status;
    atlas_video_cfg_t vcfg;
    atlas_config_t cfg;
    atlas_config_origin_t origin;
    boot_hotkeys_t keys;
    atlas_font_t *font_ui = NULL;
    atlas_font_t *font_title = NULL;
    atlas_screen_t *root;
    const char *self_path;
    atlas_err_t err;

    /*
     * argv[0] is the only place the path we were launched from exists,
     * and it is gone the moment anything else uses argv. It is stashed
     * before the IOP is touched, since a reset that fails ends the
     * program and the value would be lost for nothing.
     *
     * The path itself is validated later, once the IOP is up: at this
     * point no filesystem is mounted, so a check here would reject
     * every path on every device.
     */
    self_path = (argc > 0 && argv) ? argv[0] : NULL;

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
     * The screen comes up on safe defaults first, before the stored
     * configuration is read.
     *
     * That ordering is deliberate and costs one mode change: reading
     * the configuration needs the device layer, the device layer needs
     * time to find a Memory Card, and if that whole sequence ran before
     * anything was displayed then a console that hangs while probing a
     * failing card would hang on a black screen. Coming up first means
     * every failure after this line has somewhere to be reported.
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
     * The device layer only caches; the IOP has been enumerating USB
     * since its modules loaded, so by the time the user has read the
     * splash a stick is usually already mounted and the first poll
     * finds it.
     */
    atlas_device_init(status.memcard, status.usb);

    /* Only now can the path be opened to see whether it is real. */
    atlas_power_set_self_path(self_path);

    load_settings(&keys, &cfg, &origin);

    /*
     * Auto-launch comes after the settings that decide whether it
     * happens, and before the interface: a program that is going to
     * take the console away should not first draw a menu the user has
     * no time to use.
     *
     * Not on a recovery boot. Recovery exists because something is
     * wrong, and the stored default application is a plausible
     * candidate for what - handing the console straight to it would
     * make the escape hatch escape into the same trap.
     */
    if (!keys.recovery)
        autoboot(font_ui, &cfg);

    /*
     * From here the interface owns the frame loop.
     *
     * Three possible roots, in order of precedence:
     *
     * Recovery is a different ROOT, not a screen pushed over Home:
     * Home is drawn from a configuration that was deliberately not read
     * this boot, and a Back that fell through to it would land the user
     * in the thing they held two buttons to escape. Recovery offers
     * "Start normally" as its own entry, which is the only way from one
     * to the other.
     *
     * The wizard runs when no configuration file was found anywhere -
     * not when one was found and was damaged. A card whose ATLAS.INI
     * failed to parse is recovered from its .BAK and reported as
     * RECOVERED or PARTIAL; asking that user to set their language up
     * again would be treating a repaired file as an absent one.
     *
     * Otherwise the startup setting picks between Home and the
     * application list, which is what it exists to do.
     */
    atlas_ui_set_fonts(font_ui, font_title);

    if (keys.recovery)
        root = atlas_screen_recovery();
    else if (origin == ATLAS_CFG_DEFAULTS)
        root = atlas_screen_wizard();
    else if (cfg.startup == ATLAS_STARTUP_APPS)
        root = atlas_screen_apps();
    else
        root = atlas_screen_home();

    atlas_screen_reset(root);
    atlas_screen_run();

    atlas_device_shutdown();
    atlas_font_destroy(font_title);
    atlas_font_destroy(font_ui);
    atlas_input_shutdown();
    atlas_video_shutdown();

    return 0;
}
