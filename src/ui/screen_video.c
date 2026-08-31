/*
 * AtlasPS2 - screen_video.c
 * The video settings, applied live and confirmed before they are kept.
 *
 * WHY THIS SCREEN IS BUILT DIFFERENTLY FROM THE OTHERS
 * ---------------------------------------------------
 * It is the only one whose settings can destroy the means of using it.
 * A television that cannot lock onto 480p shows nothing at all, and a
 * user who cannot see the menu cannot press Back to leave it. So a
 * change to the mode or the aspect starts a countdown: if the user does
 * not press Cross from inside the new mode, the previous one comes back
 * on its own. The rule is that no press can leave a screen the user
 * cannot read, and no absence of a press can either.
 *
 * The position and margin settings are not guarded that way. They are
 * adjusted by eye, one press at a time, and they move the picture
 * rather than replacing the signal - the worst they can do is push the
 * interface towards an edge, which the user can see happening and undo
 * with the opposite direction.
 *
 * Nothing here writes to the Memory Card until Save is chosen. Trying a
 * mode and turning the console off should leave the console booting the
 * way it did before.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"

#include "atlas/config.h"
#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/log.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */

/*
 * Frames, not seconds: this screen counts its own draws rather than
 * reading a clock, and a frame is the only tick it is guaranteed to
 * get. The count is in NTSC frames (~60 Hz); on PAL the same number
 * runs 20% longer, which errs towards giving the user more time to
 * read a message on a television that is still re-syncing.
 */
#define REVERT_FRAMES 900 /* ~15 s */
#define REVERT_FPS    60

/* ------------------------------------------------------------------ */
/* Rows                                                                */
/*                                                                     */
/* Ordered by risk. The two that replace the video signal are first     */
/* because they are what a user comes here to fix; the four that only   */
/* nudge the picture follow, and the two that commit or discard are at  */
/* the bottom where they are not hit on the way past.                   */
/* ------------------------------------------------------------------ */

typedef enum {
    VID_ROW_MODE = 0,
    VID_ROW_ASPECT,
    VID_ROW_OFFSET_X,
    VID_ROW_OFFSET_Y,
    VID_ROW_OVERSCAN_X,
    VID_ROW_OVERSCAN_Y,
    VID_ROW_RESET,
    VID_ROW_SAVE,
    VID_ROW_COUNT
} vid_row_t;

typedef struct {
    vid_row_t      row;
    atlas_str_id_t label;
    atlas_str_id_t detail;
} vid_entry_t;

static const vid_entry_t s_entries[VID_ROW_COUNT] = {
    { VID_ROW_MODE,       ATLAS_STR_VID_MODE,       ATLAS_STR_VID_D_MODE     },
    { VID_ROW_ASPECT,     ATLAS_STR_VID_ASPECT,     ATLAS_STR_VID_D_ASPECT   },
    { VID_ROW_OFFSET_X,   ATLAS_STR_VID_OFFSET_X,   ATLAS_STR_VID_D_OFFSET   },
    { VID_ROW_OFFSET_Y,   ATLAS_STR_VID_OFFSET_Y,   ATLAS_STR_VID_D_OFFSET   },
    { VID_ROW_OVERSCAN_X, ATLAS_STR_VID_OVERSCAN_X, ATLAS_STR_VID_D_OVERSCAN },
    { VID_ROW_OVERSCAN_Y, ATLAS_STR_VID_OVERSCAN_Y, ATLAS_STR_VID_D_OVERSCAN },
    { VID_ROW_RESET,      ATLAS_STR_VID_RESET,      ATLAS_STR_VID_D_RESET    },
    { VID_ROW_SAVE,       ATLAS_STR_VID_SAVE,       ATLAS_STR_VID_D_SAVE     }
};

typedef struct {
    int cursor;

    /** What the screen is currently showing, live. */
    atlas_video_cfg_t cur;

    /**
     * What to go back to if the user cannot confirm.
     *
     * Taken from the live video module rather than from ATLAS.INI: on a
     * boot where R1 skipped the stored settings, the file names a mode
     * that was deliberately not applied, and reverting to it would put
     * the user in the mode they held a button to escape.
     */
    atlas_video_cfg_t safe;

    /** Frames left before the revert fires, or 0 when not counting. */
    int revert_left;

    /** Result of the last Save, or ATLAS_STR_COUNT for nothing to say. */
    atlas_str_id_t status;
    int            status_bad;
} vid_state_t;

static vid_state_t s_state;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void say(vid_state_t *st, atlas_str_id_t message, int bad)
{
    st->status = message;
    st->status_bad = bad;
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/**
 * Apply `st->cur` and start the countdown that undoes it.
 *
 * `safe` is left alone: it is what the screen was showing before the
 * user started changing things, and it stays that until a confirmation
 * makes the current settings the new fallback. Otherwise two changes in
 * a row would make the second one revert to the first - which may be
 * just as unreadable as the one being escaped.
 */
static void try_mode(vid_state_t *st)
{
    atlas_video_apply(&st->cur);
    st->revert_left = REVERT_FRAMES;
    say(st, ATLAS_STR_COUNT, 0);
}

/** Take the current settings as the ones to fall back to. */
static void keep_mode(vid_state_t *st)
{
    st->safe = st->cur;
    st->revert_left = 0;
}

static void revert_mode(vid_state_t *st)
{
    ATLAS_LOG("VIDEO", "not confirmed: reverting display mode");

    st->cur = st->safe;
    st->revert_left = 0;

    atlas_video_apply(&st->cur);
    say(st, ATLAS_STR_VID_REVERTED, 1);
}

/**
 * Write the settings to ATLAS.INI.
 *
 * Reads the file first and replaces only the [video] block, so a save
 * from this screen does not overwrite a language or a default
 * application the user set elsewhere. A configuration that could not be
 * read leaves the defaults in the other fields, which is the same thing
 * the next boot would have used anyway.
 */
static void save_settings(vid_state_t *st)
{
    atlas_config_t cfg;
    atlas_config_origin_t origin;

    atlas_config_load(&cfg, &origin);

    cfg.video = st->cur;

    if (atlas_config_save(&cfg) == ATLAS_OK)
        say(st, ATLAS_STR_VID_SAVED, 0);
    else
        say(st, ATLAS_STR_VID_E_SAVE, 1);
}

/* ------------------------------------------------------------------ */
/* Editing                                                             */
/* ------------------------------------------------------------------ */

/**
 * Left/Right on the selected row.
 *
 * @param dir -1 or +1.
 * @return non-zero when the change replaced the video signal, so the
 *         caller knows to start the confirmation countdown.
 */
static int adjust(vid_state_t *st, int dir)
{
    switch (s_entries[st->cursor].row) {
    case VID_ROW_MODE:
        st->cur.mode = (atlas_vmode_t)
            ((st->cur.mode + ATLAS_VMODE_COUNT + dir) % ATLAS_VMODE_COUNT);
        return 1;

    case VID_ROW_ASPECT:
        st->cur.aspect = (atlas_aspect_t)
            ((st->cur.aspect + ATLAS_ASPECT_COUNT + dir) % ATLAS_ASPECT_COUNT);
        /*
         * Only the UI's own x-scale changes, not the signal - but it
         * goes through the same confirmation, because a 16:9 layout on
         * a 4:3 set is squashed enough to be hard to read, and the
         * countdown is the thing that gets the user out of it.
         */
        return 1;

    case VID_ROW_OFFSET_X:
        st->cur.offset_x = clamp_int(st->cur.offset_x + dir,
                                     -ATLAS_CFG_OFFSET_LIMIT,
                                     ATLAS_CFG_OFFSET_LIMIT);
        break;

    case VID_ROW_OFFSET_Y:
        st->cur.offset_y = clamp_int(st->cur.offset_y + dir,
                                     -ATLAS_CFG_OFFSET_LIMIT,
                                     ATLAS_CFG_OFFSET_LIMIT);
        break;

    case VID_ROW_OVERSCAN_X:
        st->cur.overscan_x = clamp_int(st->cur.overscan_x + dir,
                                       0, ATLAS_CFG_OVERSCAN_LIMIT);
        break;

    case VID_ROW_OVERSCAN_Y:
        st->cur.overscan_y = clamp_int(st->cur.overscan_y + dir,
                                       0, ATLAS_CFG_OVERSCAN_LIMIT);
        break;

    default:
        /* Reset and Save are pressed, not scrolled through. */
        return 0;
    }

    /*
     * The four trim settings move the picture without re-opening the
     * screen. A mode switch per press would black the television out
     * between every step of an adjustment made by eye.
     */
    atlas_video_set_trim(st->cur.offset_x, st->cur.offset_y,
                         st->cur.overscan_x, st->cur.overscan_y);

    /* The saved file no longer matches what is on screen. */
    say(st, ATLAS_STR_COUNT, 0);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static void vid_enter(atlas_screen_t *self)
{
    vid_state_t *st = (vid_state_t *)self->data;

    /*
     * Both copies come from the running video module, not from the
     * configuration file: what the user is looking at is what they are
     * editing, and it is also the only fallback known to work on this
     * television.
     */
    st->cur  = *atlas_video_cfg();
    st->safe = st->cur;

    st->revert_left = 0;
    st->cursor = 0;
    st->status = ATLAS_STR_COUNT;
    st->status_bad = 0;
}

static void vid_update(atlas_screen_t *self)
{
    vid_state_t *st = (vid_state_t *)self->data;
    u32 rep = atlas_input_repeated();

    /*
     * The countdown owns the input while it runs. Cross keeps the new
     * mode, Circle goes back immediately rather than making a user who
     * can see the screen sit through the rest of the timer, and nothing
     * else does anything - including Back, which would otherwise leave
     * the screen with a revert still pending and nowhere to fire from.
     */
    if (st->revert_left > 0) {
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
            keep_mode(st);
        } else if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
            revert_mode(st);
        } else if (--st->revert_left == 0) {
            revert_mode(st);
        }

        return;
    }

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + VID_ROW_COUNT - 1) % VID_ROW_COUNT;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % VID_ROW_COUNT;

    /*
     * Adjustment uses repeated(): holding Right should walk an offset
     * across its range rather than asking for 32 presses.
     */
    if (rep & ATLAS_BTN_LEFT) {
        if (adjust(st, -1))
            try_mode(st);
    } else if (rep & ATLAS_BTN_RIGHT) {
        if (adjust(st, +1))
            try_mode(st);
    }

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        switch (s_entries[st->cursor].row) {
        case VID_ROW_RESET:
            /*
             * Only the screen's own copy: the file is untouched until
             * Save, so a reset the user did not mean costs them the
             * trip back through this menu, not their settings.
             */
            atlas_video_cfg_defaults(&st->cur);
            atlas_video_apply(&st->cur);
            st->revert_left = REVERT_FRAMES;
            say(st, ATLAS_STR_COUNT, 0);
            break;

        case VID_ROW_SAVE:
            save_settings(st);
            break;

        default:
            /* Cross on a value row does nothing; Left/Right change it.
             * A press that silently did the same as Right would make
             * the row's behaviour depend on which button was nearest. */
            break;
        }
    }

    if (atlas_input_is_pressed(ATLAS_BTN_BACK))
        atlas_screen_pop();
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/** The right-hand value for one row, as text. */
static const char *row_value(const vid_state_t *st, vid_row_t row, char *buf,
                             int size)
{
    switch (row) {
    case VID_ROW_MODE:
        /*
         * AUTO is shown with what it resolved to, because "auto" alone
         * does not tell a user whose picture rolls whether the console
         * decided PAL or NTSC - which is the first thing they need to
         * know.
         */
        if (st->cur.mode == ATLAS_VMODE_AUTO) {
            snprintf(buf, size, "auto (%s)",
                     atlas_video_mode_label(atlas_video_mode()));
        } else {
            snprintf(buf, size, "%s",
                     atlas_video_mode_label(st->cur.mode));
        }
        return buf;

    case VID_ROW_ASPECT:
        if (st->cur.aspect == ATLAS_ASPECT_AUTO) {
            snprintf(buf, size, "auto (%s)",
                     atlas_video_aspect_label(atlas_video_aspect()));
        } else {
            snprintf(buf, size, "%s",
                     atlas_video_aspect_label(st->cur.aspect));
        }
        return buf;

    case VID_ROW_OFFSET_X:
        snprintf(buf, size, "%+d", st->cur.offset_x);
        return buf;

    case VID_ROW_OFFSET_Y:
        snprintf(buf, size, "%+d", st->cur.offset_y);
        return buf;

    case VID_ROW_OVERSCAN_X:
        snprintf(buf, size, "%d", st->cur.overscan_x);
        return buf;

    case VID_ROW_OVERSCAN_Y:
        snprintf(buf, size, "%d", st->cur.overscan_y);
        return buf;

    default:
        /* Reset and Save are actions; a value beside them would read as
         * a setting they carry. */
        return NULL;
    }
}

static void vid_draw(atlas_screen_t *self)
{
    vid_state_t *st = (vid_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i;
    char val[32];
    char line[128];

    atlas_ui_header(atlas_video_mode_name());

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_HOME_VIDEO));
    y += lh * 1.9f;

    for (i = 0; i < VID_ROW_COUNT; i++) {
        atlas_ui_menu_row(x, y, w * 0.70f, i == st->cursor,
                          atlas_str(s_entries[i].label),
                          row_value(st, s_entries[i].row, val, sizeof(val)));

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    y += lh * 0.4f;
    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(s_entries[st->cursor].detail), w);
    y += lh * 1.3f;

    /*
     * Stated on the screen that offers the modes, not only in the
     * documentation: a user selecting 480p is exactly the user about to
     * assume it makes the console render more pixels.
     */
    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(ATLAS_STR_VID_NOTE), w);

    if (st->status != ATLAS_STR_COUNT) {
        y += lh * 1.3f;
        atlas_ui_text_clipped(x, y, st->status_bad ? t->warn : t->ok,
                              atlas_str(st->status), w);
    }

    snprintf(line, sizeof(line), "<>  %s     X  %s     O  %s",
             atlas_str(ATLAS_STR_CHANGE),
             atlas_str(ATLAS_STR_SELECT),
             atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(line);

    if (st->revert_left > 0) {
        /*
         * Drawn last, over everything: on a television that is barely
         * holding the new mode this box may be the only legible thing
         * on the screen, and it is the one thing the user must read.
         */
        snprintf(line, sizeof(line), atlas_str(ATLAS_STR_VID_KEEP_BODY),
                 (st->revert_left + REVERT_FPS - 1) / REVERT_FPS);

        snprintf(val, sizeof(val), "X  %s", atlas_str(ATLAS_STR_OK));

        atlas_ui_message_box(atlas_str(ATLAS_STR_VID_KEEP), line, val);
    }
}

static atlas_screen_t s_screen = {
    "Video",
    vid_enter,
    NULL,
    vid_update,
    vid_draw,
    &s_state
};

atlas_screen_t *atlas_screen_video(void)
{
    return &s_screen;
}
