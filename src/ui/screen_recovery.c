/*
 * AtlasPS2 - screen_recovery.c
 * The recovery root, reached by holding L1+R1 while the console boots.
 *
 * WHAT MAKES THIS DIFFERENT FROM EVERY OTHER SCREEN
 * -------------------------------------------------
 * It is the screen a user reaches when AtlasPS2 is what is broken. So
 * it assumes nothing: no configuration has been read (main.c skips the
 * file entirely in recovery), no theme has been loaded, and the first
 * thing enter() does is force the compiled-in theme back on in case
 * something earlier in the boot managed to install one.
 *
 * Everything it offers is either a read, or a write that has a way
 * back. Reset writes the defaults through the same atomic path the
 * settings screen uses, so the old file survives as ATLAS.INI.BAK.
 * Rollback and Update run the install engine, which never writes over
 * a working BOOT.ELF - it stages, verifies, then swaps.
 *
 * It is a ROOT, not a screen pushed over Home. Home is drawn from a
 * configuration that may be exactly what is wrong, and a Back that fell
 * through to it would land the user in the thing they were escaping.
 */
#include <stdio.h>

#include "atlas/screens.h"

#include "atlas/config.h"
#include "atlas/device.h"
#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/install.h"
#include "atlas/log.h"
#include "atlas/power.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

/* ------------------------------------------------------------------ */
/* Entries                                                             */
/*                                                                     */
/* Fixed list, safest first. The two that run the install engine sit    */
/* below the two that only touch a settings file, so a user pressing    */
/* down the list meets the reversible options before the ones that      */
/* change what the console boots.                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    REC_CONTINUE = 0,
    REC_RESET_CFG,
    REC_NO_THEME,
    REC_ROLLBACK,
    REC_UPDATE,
    REC_SWITCH,
    REC_BROWSER,
    REC_COUNT
} rec_action_t;

typedef struct {
    rec_action_t   action;
    atlas_str_id_t label;
    atlas_str_id_t detail;
} rec_entry_t;

static const rec_entry_t s_entries[REC_COUNT] = {
    { REC_CONTINUE,  ATLAS_STR_REC_CONTINUE,  ATLAS_STR_REC_D_CONTINUE  },
    { REC_RESET_CFG, ATLAS_STR_REC_RESET_CFG, ATLAS_STR_REC_D_RESET_CFG },
    { REC_NO_THEME,  ATLAS_STR_REC_NO_THEME,  ATLAS_STR_REC_D_NO_THEME  },
    { REC_ROLLBACK,  ATLAS_STR_REC_ROLLBACK,  ATLAS_STR_REC_D_ROLLBACK  },
    { REC_UPDATE,    ATLAS_STR_REC_UPDATE,    ATLAS_STR_REC_D_UPDATE    },
    { REC_SWITCH,    ATLAS_STR_REC_SWITCH,    ATLAS_STR_REC_D_SWITCH    },
    { REC_BROWSER,   ATLAS_STR_REC_BROWSER,   ATLAS_STR_REC_D_BROWSER   }
};

typedef struct {
    int cursor;
    int confirming;

    /** Which card the two engine operations act on. */
    atlas_device_id_t target;

    /** Result of the last action, or ATLAS_STR_COUNT for none yet. */
    atlas_str_id_t status;

    /** Non-zero when `status` reports a failure rather than a success. */
    int status_bad;
} rec_state_t;

static rec_state_t s_state;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static atlas_device_id_t first_card(void)
{
    if (atlas_device_is_ready(ATLAS_DEV_MC0))
        return ATLAS_DEV_MC0;

    if (atlas_device_is_ready(ATLAS_DEV_MC1))
        return ATLAS_DEV_MC1;

    return ATLAS_DEV_MC0;
}

static void say(rec_state_t *st, atlas_str_id_t message, int bad)
{
    st->status = message;
    st->status_bad = bad;
}

/** Whether an entry can do anything on the card currently selected. */
static int available(const rec_state_t *st, rec_action_t action)
{
    switch (action) {
    case REC_ROLLBACK:
        return atlas_install_op_available(ATLAS_OP_ROLLBACK, st->target);

    case REC_UPDATE:
        return atlas_install_op_available(ATLAS_OP_UPDATE, st->target);

    default:
        /* The rest need no card at all. */
        return 1;
    }
}

/**
 * Actions that change what the console boots, or that leave AtlasPS2.
 *
 * Reset and Disable theme are not in the list: both are recoverable
 * from within this same screen, and a confirmation on every entry
 * teaches a user to press through the one that matters.
 */
static int needs_confirmation(rec_action_t action)
{
    return action == REC_ROLLBACK || action == REC_UPDATE
        || action == REC_BROWSER;
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

/**
 * Write the defaults over ATLAS.INI.
 *
 * atlas_config_save() goes through the atomic write, which rotates the
 * previous file to ATLAS.INI.BAK before putting the new one in place -
 * so this is reversible with a card reader, which is exactly the
 * promise the on-screen description makes.
 */
static void reset_config(rec_state_t *st)
{
    atlas_config_t cfg;

    atlas_config_defaults(&cfg);

    if (atlas_config_save(&cfg) == ATLAS_OK)
        say(st, ATLAS_STR_REC_DONE_CFG, 0);
    else
        say(st, ATLAS_STR_REC_E_FAILED, 1);
}

/**
 * Stop a custom theme being loaded on the next boot.
 *
 * Reads the file first and rewrites only the theme name, because the
 * rest of it is the user's - clearing their video mode to fix their
 * colours would be a repair that breaks something else.
 *
 * A configuration that could not be read has no theme name in it to
 * clear, so there is nothing to write and the built-in appearance is
 * already what will come up. Saying so is honest; writing a default
 * file would quietly do what Reset does, from the entry that promises
 * it deletes nothing.
 */
static void disable_theme(rec_state_t *st)
{
    atlas_config_t cfg;
    atlas_config_origin_t origin;

    /* The active theme, in case something already installed one. */
    atlas_theme_set(NULL);

    atlas_config_load(&cfg, &origin);

    if (origin == ATLAS_CFG_DEFAULTS) {
        say(st, ATLAS_STR_REC_DONE_THEME, 0);
        return;
    }

    cfg.theme[0] = '\0';

    if (atlas_config_save(&cfg) == ATLAS_OK)
        say(st, ATLAS_STR_REC_DONE_THEME, 0);
    else
        say(st, ATLAS_STR_REC_E_FAILED, 1);
}

static void start_job(rec_state_t *st, atlas_install_op_t op)
{
    atlas_install_job_t job;

    atlas_install_begin(&job, op, st->target);
    atlas_screen_push(atlas_screen_install_run(&job));
}

/**
 * Install the build sitting in mass:/ATLAS_UPDATE/.
 *
 * The engine searches USB before the cards and leads with the update
 * folder, so this is the same operation the installer performs - but
 * the missing-stick case is caught here rather than on the progress
 * screen. "No update found" belongs beside the entry the user pressed,
 * not behind a screen transition that looks like the copy started.
 */
static void start_update(rec_state_t *st)
{
    char source[ATLAS_INSTALL_PATH_MAX];

    if (atlas_install_find_source(source, sizeof(source)) != ATLAS_OK) {
        say(st, ATLAS_STR_REC_E_NOUSB, 1);
        return;
    }

    start_job(st, ATLAS_OP_UPDATE);
}

static void perform(rec_state_t *st, rec_action_t action)
{
    switch (action) {
    case REC_CONTINUE:
        /*
         * Hands the frame loop to the normal interface without reading
         * the configuration. Recovery skipped that file on purpose, and
         * a "start normally" that loaded it would run the settings this
         * mode exists to escape. They are trusted again on the next
         * boot, which is when the user has had the chance to fix them.
         */
        ATLAS_LOG("REC", "leaving recovery for the normal interface");
        atlas_screen_reset(atlas_screen_home());
        break;

    case REC_RESET_CFG:
        reset_config(st);
        break;

    case REC_NO_THEME:
        disable_theme(st);
        break;

    case REC_ROLLBACK:
        start_job(st, ATLAS_OP_ROLLBACK);
        break;

    case REC_UPDATE:
        start_update(st);
        break;

    case REC_SWITCH:
        /* Straight swap. An empty other slot may be selected: the
         * status block then says it is unusable, which tells the user
         * more than a button that appears not to work. */
        st->target = (st->target == ATLAS_DEV_MC0) ? ATLAS_DEV_MC1
                                                   : ATLAS_DEV_MC0;
        say(st, ATLAS_STR_COUNT, 0);
        break;

    case REC_BROWSER:
    default:
        atlas_power_exit_to_browser();

        /* Only reached if the exit failed; the menu stays up. */
        say(st, ATLAS_STR_REC_E_FAILED, 1);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static void rec_enter(atlas_screen_t *self)
{
    rec_state_t *st = (rec_state_t *)self->data;

    /*
     * Forced every time this screen becomes current, not just on the
     * first entry: a theme file is the kind of thing a user comes here
     * to escape, and it must not be able to survive a trip through the
     * progress screen and back.
     */
    atlas_theme_set(NULL);

    if (!atlas_device_is_ready(st->target))
        st->target = first_card();

    /* Coming back from a finished job, whatever it reported is on that
     * screen and this one's own status line is stale. */
    st->status = ATLAS_STR_COUNT;
    st->status_bad = 0;
    st->confirming = 0;
}

static void rec_update(atlas_screen_t *self)
{
    rec_state_t *st = (rec_state_t *)self->data;
    u32 rep = atlas_input_repeated();

    if (st->confirming) {
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
            st->confirming = 0;
            perform(st, s_entries[st->cursor].action);
        } else if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
            st->confirming = 0;
        }

        return;
    }

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + REC_COUNT - 1) % REC_COUNT;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % REC_COUNT;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
        rec_action_t action = s_entries[st->cursor].action;

        if (!available(st, action))
            return;

        if (needs_confirmation(action))
            st->confirming = 1;
        else
            perform(st, action);
    }

    /*
     * No Back handler. This is the root, and there is nothing
     * underneath it - the way out is "Start normally" or "Return to PS2
     * Browser", both of which are entries a user can read before
     * choosing.
     */
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/** The card this screen's engine operations will act on. */
static void draw_target(const rec_state_t *st, float x, float y, float w)
{
    const atlas_theme_t *t = atlas_theme();
    const atlas_device_t *d = atlas_device_get(st->target);
    int ready = atlas_device_is_ready(st->target);

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim,
                  atlas_str(ATLAS_STR_REC_TARGET));

    atlas_ui_text(x + w, y, ATLAS_ALIGN_RIGHT,
                  ready ? t->text : t->warn,
                  d && d->name ? d->name : "?");
}

static void rec_draw(atlas_screen_t *self)
{
    rec_state_t *st = (rec_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i;
    char hints[96];

    atlas_ui_header(ATLAS_VERSION_STRING);

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->warn,
                        atlas_str(ATLAS_STR_REC_TITLE));
    y += lh * 1.8f;

    /* Says why the console looks like this before it says what can be
     * done about it: a user who held two buttons by accident needs to
     * recognise the situation, not read a menu. */
    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(ATLAS_STR_REC_INTRO), w);
    y += lh * 1.6f;

    draw_target(st, x, y, w);
    y += lh * 1.4f;

    atlas_ui_separator(x, y, w, t->separator);
    y += lh * 0.7f;

    for (i = 0; i < REC_COUNT; i++) {
        const rec_entry_t *e = &s_entries[i];

        /*
         * An unusable entry still draws and can still be selected; it
         * just does nothing when pressed, and carries the reason as its
         * right-hand value. A row that vanished as a card was swapped
         * would make the menu change shape under the user.
         */
        atlas_ui_menu_row(x, y, w * 0.62f, i == st->cursor,
                          atlas_str(e->label),
                          available(st, e->action)
                              ? NULL : atlas_str(ATLAS_STR_INS_NONE));

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    y += lh * 0.4f;
    atlas_ui_text_clipped(x, y, t->text_dim,
                          atlas_str(s_entries[st->cursor].detail), w);

    if (st->status != ATLAS_STR_COUNT) {
        y += lh * 1.4f;
        atlas_ui_text_clipped(x, y, st->status_bad ? t->error : t->ok,
                              atlas_str(st->status), w);
    }

    snprintf(hints, sizeof(hints), "X  %s", atlas_str(ATLAS_STR_SELECT));
    atlas_ui_footer(hints);

    if (st->confirming) {
        snprintf(hints, sizeof(hints), "X  %s     O  %s",
                 atlas_str(ATLAS_STR_CONFIRM),
                 atlas_str(ATLAS_STR_CANCEL));
        atlas_ui_message_box(atlas_str(s_entries[st->cursor].label),
                             atlas_str(s_entries[st->cursor].detail),
                             hints);
    }
}

static atlas_screen_t s_screen = {
    "Recovery",
    rec_enter,
    NULL,
    rec_update,
    rec_draw,
    &s_state
};

atlas_screen_t *atlas_screen_recovery(void)
{
    return &s_screen;
}
