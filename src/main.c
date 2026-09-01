/*
 * AtlasPS2 - main.c
 * Entry point and the top-level boot sequence.
 */
#include <stdio.h>
#include <string.h>

#include <gsKit.h>
#include <kernel.h>
#include <libpad.h>
#include <delaythread.h>

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

/*
 * The background broken out into components as well as a packed colour:
 * the splash fades by interpolating towards it, which needs the parts.
 */
#define BG_R 0x12
#define BG_G 0x14
#define BG_B 0x18

#define COL_BG       RGB(BG_R, BG_G, BG_B)
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

/*
 * How long to keep sampling for a held hotkey, in microseconds, and how
 * long to pause between samples.
 *
 * This used to be a count of sixty "frames" taken before video existed,
 * which meant no frames and no waiting: the whole loop elapsed in
 * microseconds and could not observe a pad that had not finished
 * negotiating. Time is what was meant, so time is what is counted.
 *
 * atlas_input_init() has already waited for the pad to become ready by
 * the time this runs, so this window is only about giving a human hand
 * time to be seen - a quarter of a second, sampled every few
 * milliseconds.
 */
#define HOTKEY_WINDOW_US   250000
#define HOTKEY_INTERVAL_US   4000

typedef struct {
    int recovery;   /* L1 + R1: minimal UI, no theme, no config      */
    int safe_video; /* R1 alone: force NTSC 4:3 with no user offsets */
} boot_hotkeys_t;

static boot_hotkeys_t read_hotkeys(void)
{
    boot_hotkeys_t keys = {0, 0};
    int waited;

    /*
     * Sample repeatedly and latch anything seen, rather than testing
     * once: the user is holding the buttons across the whole window,
     * and a single sample can land in the gap between two IOP updates.
     */
    for (waited = 0; waited < HOTKEY_WINDOW_US;
         waited += HOTKEY_INTERVAL_US) {
        u16 raw;

        atlas_input_update();
        raw = atlas_input_raw();

        if ((raw & PAD_L1) && (raw & PAD_R1))
            keys.recovery = 1;
        else if (raw & PAD_R1)
            keys.safe_video = 1;

        if (keys.recovery)
            break; /* recovery wins; no point sampling further */

        DelayThread(HOTKEY_INTERVAL_US);
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

/*
 * Splash timing, in frames. Interlaced NTSC and PAL differ (60 vs 50),
 * so this is a little longer on an NTSC console and a little shorter on
 * a PAL one; nobody can tell, and the alternative is a timer dependency
 * for something whose only requirement is "about a second and a half".
 */
#define SPLASH_FRAMES     90
#define SPLASH_FADE_IN    12
#define SPLASH_FADE_OUT   14

/**
 * Scale a colour's RGB towards the background by a 0..1 factor.
 *
 * The GS blends against what is already in the framebuffer, and the
 * splash draws over a cleared background rather than compositing, so a
 * fade is done by moving the colour itself rather than by changing
 * alpha - which on this hardware would need a blend mode the rest of
 * the frame does not use.
 */
static u64 fade_color(u64 color, float f)
{
    u32 r = (u32)((color >>  0) & 0xFF);
    u32 g = (u32)((color >>  8) & 0xFF);
    u32 b = (u32)((color >> 16) & 0xFF);

    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;

    r = (u32)(BG_R + ((float)r - BG_R) * f);
    g = (u32)(BG_G + ((float)g - BG_G) * f);
    b = (u32)(BG_B + ((float)b - BG_B) * f);

    return GS_SETREG_RGBAQ(r, g, b, 0x80, 0x00);
}

/**
 * Boot splash: the program's name, its version, and nothing else.
 *
 * It used to list which IOP modules had loaded and wait for a button.
 * That is a developer's diagnostic screen, shown to every user on every
 * boot, gating the console behind a keypress to read something they did
 * not ask for - and the same information is in Settings, where somebody
 * looking for it can find it. What a boot splash owes the user is proof
 * the console is alive, briefly, and then the interface.
 *
 * So it fades in, holds, fades out, and returns on its own. Any button
 * skips the rest, because a splash nobody can dismiss is the same
 * mistake in the other direction.
 *
 * A boot that is not normal still gets a word: recovery and safe video
 * are states the user chose with a button held at power-on, and one of
 * them is how somebody escapes a console that will not display. Leaving
 * that unsaid would mean the escape hatch works silently, which is
 * indistinguishable from it not working.
 *
 * Drawn with the fonts directly rather than through the UI layer: this
 * runs before the theme is read, and it must survive a theme that is
 * missing or broken.
 */
static void splash_loop(atlas_font_t *title, atlas_font_t *ui,
                        const atlas_boot_status_t *st,
                        const boot_hotkeys_t *keys)
{
    int frame;
    float cy, half_h, scale;

    (void)st; /* module status belongs in Settings, not on a splash */

    /*
     * Two coordinate systems meet here. draw_centered() works in
     * framebuffer pixels, atlas_ui_rect() in safe-area units that it
     * scales itself for 16:9 - so the rule's geometry is divided by
     * that scale to land under a title placed in the other system.
     */
    scale  = atlas_video_x_scale();
    half_h = (float)atlas_video_safe_h() * 0.5f;
    cy     = (float)atlas_video_safe_y() + half_h;

    for (frame = 0; frame < SPLASH_FRAMES; frame++) {
        float f = 1.0f;
        float rule_w;

        atlas_input_update();

        /*
         * Any button, not CONFIRM specifically. This screen asks for
         * nothing, so there is no answer to give - a user pressing
         * something wants it gone, whichever button they reached for.
         *
         * Read as "held", not as an edge: a user who was already
         * holding a button when the splash appeared - which is exactly
         * what happens when they held R1 for safe video - would
         * otherwise have to release it and press again.
         */
        if (frame > SPLASH_FADE_IN && atlas_input_held() != 0)
            break;

        if (frame < SPLASH_FADE_IN)
            f = (float)frame / (float)SPLASH_FADE_IN;
        else if (frame > SPLASH_FRAMES - SPLASH_FADE_OUT)
            f = (float)(SPLASH_FRAMES - frame) / (float)SPLASH_FADE_OUT;

        atlas_video_frame_begin(COL_BG);

        /*
         * Name, a hairline, then the version under it. The rule is as
         * wide as the title and sits between the two, which is what
         * makes the pair read as one object rather than two strings
         * that happen to share a centre.
         */
        draw_centered(title, cy - 34.0f, fade_color(COL_TEXT, f),
                      ATLAS_NAME);

        rule_w = atlas_font_width(title, ATLAS_NAME) / scale;
        atlas_ui_rect(((float)atlas_video_safe_w() / scale - rule_w) * 0.5f,
                      half_h + 4.0f, rule_w, 1.0f,
                      fade_color(COL_ACCENT, f * 0.7f));

        draw_centered(ui, cy + 16.0f, fade_color(COL_DIM, f),
                      ATLAS_VERSION_STRING);

        if (keys->recovery || keys->safe_video) {
            draw_centered(ui,
                          (float)(atlas_video_safe_y()
                                  + atlas_video_safe_h())
                              - atlas_font_line_height(ui) * 2.0f,
                          fade_color(COL_WARN, f),
                          keys->recovery ? "Recovery mode"
                                         : "Safe video mode");
        }

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

    /*
     * Tried unconditionally, not only when the module load reported
     * success.
     *
     * SifExecModuleBuffer() reports a failure when the module is
     * already resident, which is what happens when the loader that
     * launched us left its own PADMAN behind - a working pad reported
     * as a failed one. Gating on that verdict skipped pad
     * initialisation entirely and left an interface that drew correctly
     * and answered nothing.
     *
     * The reverse is cheap: on a console that really has no PADMAN,
     * padInit() returns a negative value and this gives up in
     * milliseconds.
     */
    status.pad = (atlas_input_init() == ATLAS_OK);

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
