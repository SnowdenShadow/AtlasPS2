/*
 * AtlasPS2 - screen_sysinfo.c
 * What came up at boot, and what did not.
 */
#include <stdio.h>

#include "atlas/screens.h"
#include "atlas/ui.h"
#include "atlas/input.h"
#include "atlas/video.h"
#include "atlas/boot.h"
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
 */

static void row(float x, float *y, float w, const char *label,
                const char *value, u64 value_color)
{
    const atlas_theme_t *t = atlas_theme();
    float lh = atlas_ui_line_height();

    atlas_ui_text(x, *y, ATLAS_ALIGN_LEFT, t->text_dim, label);
    atlas_ui_text(x + w, *y, ATLAS_ALIGN_RIGHT, value_color, value);

    *y += lh * 1.35f;
}

static void status_row(float x, float *y, float w, const char *label, int ok)
{
    const atlas_theme_t *t = atlas_theme();

    row(x, y, w, label,
        atlas_str(ok ? ATLAS_STR_SYS_AVAILABLE : ATLAS_STR_SYS_UNAVAILABLE),
        ok ? t->ok : t->text_dim);
}

static void sysinfo_update(atlas_screen_t *self)
{
    if (atlas_input_is_pressed(ATLAS_BTN_BACK))
        atlas_screen_pop();
}

static void sysinfo_draw(atlas_screen_t *self)
{
    const atlas_boot_status_t *st = atlas_boot_status();
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float y;
    char buf[64];

    atlas_ui_header(atlas_str(ATLAS_STR_HOME_SYSINFO));

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_SYSINFO));
    y += atlas_ui_line_height() * 2.2f;

    row(x, &y, w, atlas_str(ATLAS_STR_SYS_VERSION), ATLAS_VERSION_STRING,
        t->accent);
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_VIDEO_MODE),
        atlas_video_mode_name(), t->text);

    snprintf(buf, sizeof(buf), "%d x %d",
             atlas_video_width(), atlas_video_height());
    row(x, &y, w, atlas_str(ATLAS_STR_SYS_RESOLUTION), buf, t->text);

    row(x, &y, w, atlas_str(ATLAS_STR_SYS_ASPECT),
        atlas_video_aspect_label(atlas_video_aspect()), t->text);

    y += atlas_ui_line_height() * 0.5f;
    atlas_ui_separator(x, y, w, t->separator);
    y += atlas_ui_line_height() * 0.8f;

    status_row(x, &y, w, "File system (iomanX, fileXio)", st->fileio);
    status_row(x, &y, w, "Controllers (sio2man, padman)", st->pad);
    status_row(x, &y, w, "Memory cards (mcman, mcserv)", st->memcard);
    status_row(x, &y, w, "USB storage (bdm, fatfs)", st->usb);
    status_row(x, &y, w, "Power off", st->poweroff);

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
