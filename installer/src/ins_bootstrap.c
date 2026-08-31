/*
 * AtlasPS2 - ins_bootstrap.c
 * Why this program will not install a bootstrap or exploit.
 *
 * This is a full screen rather than a footnote because the question it
 * answers is the one that strands users: they install AtlasPS2 onto a
 * card, restart, and the console boots its own menu instead. The reason
 * is that a PS2 will not run a homebrew BOOT.ELF at all until an
 * exploit has been set up, and which exploit a given console needs
 * depends on its model and ROM version.
 *
 * Guessing that is what costs people their Memory Card, and there is no
 * software test on the console itself that reliably distinguishes every
 * case. So this program refuses, says so plainly, and points at the
 * tools that do it properly - which is the spec's requirement: stop and
 * display a clear compatibility message rather than install by guess.
 */
#include <stdio.h>

#include "ins_screen.h"

#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

static void boot_update(atlas_screen_t *self)
{
    (void)self;

    if (atlas_input_is_pressed(ATLAS_BTN_BACK)
        || atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        atlas_screen_pop();
}

/**
 * Draw `text` wrapped to `w`, returning the y below the last line.
 *
 * The UI's text widgets clip rather than wrap - every other screen has
 * short labels - and this one paragraph is the exception. Wrapping is
 * done here rather than added to atlas_ui_text_clipped() because a
 * widget that silently wrapped would break the menu rows that rely on
 * being clipped to one line.
 */
static float wrapped(float x, float y, float w, u64 color, const char *text)
{
    char line[128];
    float lh = atlas_ui_line_height();
    int i = 0, cut = 0, n = 0;

    while (text[i] != '\0') {
        /* Take one more character, remembering the last space so the
         * line can be broken there rather than mid-word. */
        if (n < (int)sizeof(line) - 1) {
            if (text[i] == ' ')
                cut = n;

            line[n++] = text[i++];
        }

        /*
         * Break when the line no longer fits, or when the buffer is
         * full. Measuring after appending costs one character of
         * overshoot, which the break-at-space undoes.
         */
        line[n] = '\0';

        if (atlas_ui_text_width(line) <= w && n < (int)sizeof(line) - 1
            && text[i] != '\0')
            continue;

        if (text[i] != '\0' && cut > 0) {
            /* Rewind to the space and resume from just after it. */
            i -= (n - cut);
            n = cut;
            line[n] = '\0';
        }

        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, color, line);
        y += lh * 1.15f;

        while (text[i] == ' ')
            i++;

        n = 0;
        cut = 0;
    }

    if (n > 0) {
        line[n] = '\0';
        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, color, line);
        y += lh * 1.15f;
    }

    return y;
}

static void boot_draw(atlas_screen_t *self)
{
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float y;
    char hints[64];

    (void)self;

    atlas_ui_header(ATLAS_VERSION_STRING);

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->warn,
                        atlas_str(ATLAS_STR_INS_BOOT_TITLE));
    y += atlas_ui_line_height() * 2.2f;

    wrapped(x, y, w, t->text, atlas_str(ATLAS_STR_INS_BOOT_BODY));

    snprintf(hints, sizeof(hints), "O  %s", atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);
}

static atlas_screen_t s_screen = {
    "InstallerBootstrap",
    NULL,
    NULL,
    boot_update,
    boot_draw,
    NULL
};

atlas_screen_t *atlas_ins_screen_bootstrap(void)
{
    return &s_screen;
}
