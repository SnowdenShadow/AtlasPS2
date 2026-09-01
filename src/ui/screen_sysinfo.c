/*
 * AtlasPS2 - screen_sysinfo.c
 * What came up at boot, and what did not.
 */
#include <stdio.h>

#include <gsKit.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/boot.h"
#include "atlas/device.h"
#include "atlas/atlas.h"
#include "atlas/i18n.h"

/*
 * This screen exists so that "USB does not work" becomes a diagnosis
 * rather than a mystery. Every line names a subsystem and says whether
 * it is available, so a bug report can start from facts.
 *
 * The module names stay in English: "sio2man" and "bdmfs_fatfs" are
 * what the SDK calls them and what a user searching for the problem
 * will find, so translating them would make the screen less useful in
 * the language it was translated into.
 *
 * WHY TWO COLUMNS
 * ---------------
 * Everything here has to be readable at once. A user reads this screen
 * to a person helping them, or copies it into a bug report, and a list
 * that scrolls means the half they did not scroll to is the half that
 * mattered. Two columns fit the whole thing on one 448-line screen with
 * room to spare on PAL.
 *
 * WHAT IS DELIBERATELY NOT HERE
 * -----------------------------
 * The console's ROM string. atlas_install_console_id() can read it and
 * the installer shows it, because there it decides whether an install
 * is safe. Here it would be an identifier printed for no reason, which
 * the spec asks us not to do. The region - the one part of it that
 * changes what the console does - is shown instead.
 */

#define COL_GAP 24.0f

/* Rows are tighter than a menu's: this is a table to read, not a list
 * to move a cursor through. */
#define ROW_SPACING 1.28f

static void row(float x, float *y, float w, const char *label,
                const char *value, u64 value_color)
{
    const atlas_theme_t *t = atlas_theme();
    float lh = atlas_ui_line_height();

    atlas_ui_text(x, *y, ATLAS_ALIGN_LEFT, t->text_dim, label);
    atlas_ui_text(x + w, *y, ATLAS_ALIGN_RIGHT, value_color, value);

    *y += lh * ROW_SPACING;
}

static void status_row(float x, float *y, float w, const char *label, int ok)
{
    const atlas_theme_t *t = atlas_theme();

    row(x, y, w, label,
        atlas_str(ok ? ATLAS_STR_SYS_AVAILABLE : ATLAS_STR_SYS_UNAVAILABLE),
        ok ? t->ok : t->text_dim);
}

static void heading(float x, float *y, float w, const char *text)
{
    const atlas_theme_t *t = atlas_theme();
    float lh = atlas_ui_line_height();

    atlas_ui_text(x, *y, ATLAS_ALIGN_LEFT, t->accent, text);
    *y += lh * 0.95f;

    atlas_ui_separator(x, *y, w, t->separator);
    *y += lh * 0.55f;
}

/* ------------------------------------------------------------------ */
/* Facts                                                               */
/* ------------------------------------------------------------------ */

/**
 * The console's broadcast region, or NULL when it cannot be trusted.
 *
 * gsKit_check_rom() reads the ROM version block, and it is the same
 * call the video module already relies on to resolve AUTO on every
 * console it runs on. Anything finer-grained than PAL/NTSC would be a
 * guess, and a guess printed as a fact is worse than an empty row.
 */
static const char *console_region(void)
{
    return (gsKit_check_rom() == GS_MODE_PAL) ? "PAL" : "NTSC";
}

/**
 * What built this ELF.
 *
 * The spec asks for "PS2SDK build information". PS2SDK defines no
 * version macro - there is nothing in its headers to read - so what is
 * shown is what can actually be known: the compiler and the date this
 * file was compiled. Printing an invented SDK version would be worse
 * than printing nothing, because someone would eventually report a bug
 * against it.
 */
static const char *toolchain(void)
{
    return "GCC " __VERSION__;
}

static const char *build_date(void)
{
    return __DATE__;
}

/** One storage row: free space when it is known, else why not. */
static void device_row(float x, float *y, float w, atlas_device_id_t id,
                       char *buf, int size)
{
    const atlas_device_t *d = atlas_device_get(id);
    const atlas_theme_t *t = atlas_theme();

    if (!d)
        return;

    switch (d->state) {
    case ATLAS_DEV_READY:
        if (d->free_kb >= 0) {
            snprintf(buf, size, atlas_str(ATLAS_STR_DEV_FREE_KB), d->free_kb);
            row(x, y, w, d->name, buf, t->ok);
        } else {
            /*
             * USB reports no figure: a FAT volume's free space means
             * walking the allocation table, which is far too slow to do
             * on a screen that redraws every frame. Saying "ready" is
             * the honest answer to a question we did not ask.
             */
            row(x, y, w, d->name, atlas_str(ATLAS_STR_DEV_READY), t->ok);
        }
        break;

    case ATLAS_DEV_UNFORMATTED:
        row(x, y, w, d->name, atlas_str(ATLAS_STR_DEV_UNFORMATTED), t->warn);
        break;

    case ATLAS_DEV_ERROR:
        row(x, y, w, d->name, atlas_str(ATLAS_STR_DEV_ERROR), t->error);
        break;

    default:
        row(x, y, w, d->name, atlas_str(ATLAS_STR_DEV_ABSENT), t->text_dim);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Screen                                                              */
/* ------------------------------------------------------------------ */

static void sysinfo_update(atlas_screen_t *self)
{
    if (atlas_input_is_pressed(ATLAS_BTN_BACK)
        || atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        atlas_screen_pop();
}

static void sysinfo_draw(atlas_screen_t *self)
{
    const atlas_boot_status_t *st = atlas_boot_status();
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float full = sw - (float)ATLAS_UI_PAD * 2.0f;
    float w = (full - COL_GAP) * 0.5f;
    float x2 = x + w + COL_GAP;
    float top, y, y2;
    char buf[64];

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_SYSINFO));

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_SYSINFO));
    y += atlas_ui_line_height() * 2.0f;

    top = y;
    y2 = y;

    /* ---- Left: the program and the picture ---- */

    heading(x, &y, w, atlas_str(ATLAS_STR_SYS_H_SYSTEM));

    row(x, &y, w, atlas_str(ATLAS_STR_SYS_VERSION), ATLAS_VERSION_STRING,
        t->accent);
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_REGION), console_region(), t->text);
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_VIDEO_MODE),
        atlas_video_mode_name(), t->text);

    snprintf(buf, sizeof(buf), "%d x %d",
             atlas_video_width(), atlas_video_height());
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_RESOLUTION), buf, t->text);

    row(x, &y, w, atlas_str(ATLAS_STR_SYS_ASPECT),
        atlas_video_aspect_label(atlas_video_aspect()), t->text);

    row(x, &y, w, atlas_str(ATLAS_STR_SYS_TOOLCHAIN), toolchain(), t->text_dim);
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_BUILD_DATE), build_date(),
        t->text_dim);

    y += atlas_ui_line_height() * 0.6f;

    heading(x, &y, w, atlas_str(ATLAS_STR_SYS_H_NETWORK));

    /*
     * Honest rather than blank. This build loads no network modules, so
     * there is no address to report - and a row reading "unavailable"
     * would suggest a network that failed to come up rather than one
     * that was never asked for.
     */
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_IP),
        atlas_str(ATLAS_STR_SYS_NO_NET), t->text_dim);

    /* ---- Right: what is attached, and what is loaded ---- */

    heading(x2, &y2, w, atlas_str(ATLAS_STR_SYS_H_STORAGE));

    device_row(x2, &y2, w, ATLAS_DEV_MC0, buf, sizeof(buf));
    device_row(x2, &y2, w, ATLAS_DEV_MC1, buf, sizeof(buf));
    device_row(x2, &y2, w, ATLAS_DEV_MASS, buf, sizeof(buf));

    y2 += atlas_ui_line_height() * 0.6f;

    heading(x2, &y2, w, atlas_str(ATLAS_STR_SYS_H_MODULES));

    /* Shortened to fit a column, but still the SDK's own names: the
     * point of printing them is that they can be searched for. */
    status_row(x2, &y2, w, "iomanX / fileXio", st->fileio);
    status_row(x2, &y2, w, "sio2man / padman", st->pad);
    status_row(x2, &y2, w, "mcman / mcserv", st->memcard);
    status_row(x2, &y2, w, "bdm / bdmfs_fatfs", st->usb);
    status_row(x2, &y2, w, "poweroff", st->poweroff);

    ATLAS_UNUSED(top);

    snprintf(buf, sizeof(buf), "O  %s", atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(buf);
}

static atlas_screen_t s_screen = {
    "System Info",
    NULL, NULL,
    sysinfo_update,
    sysinfo_draw,
    NULL
};

atlas_screen_t *atlas_screen_sysinfo(void)
{
    return &s_screen;
}
