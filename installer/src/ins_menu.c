/*
 * AtlasPS2 - ins_menu.c
 * The installer's main screen: what was detected, and what can be done.
 */
#include <stdio.h>
#include <string.h>

#include "ins_screen.h"

#include "atlas/device.h"
#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/power.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

/* ------------------------------------------------------------------ */
/* Entries                                                             */
/*                                                                     */
/* The list is fixed and in the spec's order. Entries that cannot run  */
/* on the selected card are drawn greyed rather than removed: a menu   */
/* whose contents change as a card is swapped is hard to learn, and an */
/* option that is visibly unavailable is an explanation where a        */
/* missing one is a mystery.                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    ENTRY_OP = 0,    /* runs an atlas_install_op_t                  */
    ENTRY_CARD,      /* switches between Memory Card slots          */
    ENTRY_EXIT
} entry_kind_t;

typedef struct {
    entry_kind_t       kind;
    atlas_install_op_t op;       /* valid when kind is ENTRY_OP */
    atlas_str_id_t     label;
    atlas_str_id_t     detail;
} menu_entry_t;

static const menu_entry_t s_entries[] = {
    { ENTRY_OP,   ATLAS_OP_INSTALL,   ATLAS_STR_INS_INSTALL,
                                      ATLAS_STR_INS_D_INSTALL   },
    { ENTRY_OP,   ATLAS_OP_UPDATE,    ATLAS_STR_INS_UPDATE,
                                      ATLAS_STR_INS_D_UPDATE    },
    { ENTRY_OP,   ATLAS_OP_REPAIR,    ATLAS_STR_INS_REPAIR,
                                      ATLAS_STR_INS_D_REPAIR    },
    { ENTRY_OP,   ATLAS_OP_BACKUP,    ATLAS_STR_INS_BACKUP,
                                      ATLAS_STR_INS_D_BACKUP    },
    { ENTRY_OP,   ATLAS_OP_RESTORE,   ATLAS_STR_INS_RESTORE,
                                      ATLAS_STR_INS_D_RESTORE   },
    { ENTRY_OP,   ATLAS_OP_UNINSTALL, ATLAS_STR_INS_UNINSTALL,
                                      ATLAS_STR_INS_D_UNINSTALL },
    { ENTRY_CARD, ATLAS_OP_COUNT,     ATLAS_STR_INS_CHANGE_CARD,
                                      ATLAS_STR_COUNT           },
    { ENTRY_EXIT, ATLAS_OP_COUNT,     ATLAS_STR_INS_EXIT,
                                      ATLAS_STR_INS_D_EXIT      }
};

#define ENTRY_COUNT ((int)(sizeof(s_entries) / sizeof(s_entries[0])))

typedef struct {
    int cursor;
    atlas_device_id_t target;

    /* Confirmation for the destructive half of the menu. Held as an
     * index rather than a flag so the box can name what it is asking
     * about. */
    int confirming;

    char console[16];
    char source[ATLAS_INSTALL_PATH_MAX];
    int  have_source;
} menu_state_t;

static menu_state_t s_state;

/* ------------------------------------------------------------------ */
/* Detection                                                           */
/* ------------------------------------------------------------------ */

/** The first Memory Card slot with a usable card, or MC0 if neither. */
static atlas_device_id_t first_card(void)
{
    if (atlas_device_is_ready(ATLAS_DEV_MC0))
        return ATLAS_DEV_MC0;

    if (atlas_device_is_ready(ATLAS_DEV_MC1))
        return ATLAS_DEV_MC1;

    return ATLAS_DEV_MC0;
}

static void refresh_source(menu_state_t *st)
{
    st->have_source = atlas_install_find_source(st->source,
                                                sizeof(st->source))
                      == ATLAS_OK;
}

static void menu_enter(atlas_screen_t *self)
{
    menu_state_t *st = (menu_state_t *)self->data;

    atlas_install_console_id(st->console, sizeof(st->console));

    /*
     * Re-read on every entry, not just the first. Coming back from a
     * finished install, the card now holds AtlasPS2 and the available
     * operations are a different set - a stale menu would still be
     * offering Install.
     */
    if (!atlas_device_is_ready(st->target))
        st->target = first_card();

    refresh_source(st);
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

/**
 * Operations that change what the console boots.
 *
 * These get a second, deliberate press. The other three either only add
 * a file or only read one, and a confirmation on every entry trains a
 * user to press through the one that matters.
 */
static int needs_confirmation(atlas_install_op_t op)
{
    return op == ATLAS_OP_INSTALL || op == ATLAS_OP_UPDATE
        || op == ATLAS_OP_RESTORE || op == ATLAS_OP_UNINSTALL;
}

static void start(menu_state_t *st, atlas_install_op_t op)
{
    atlas_install_job_t job;

    atlas_install_begin(&job, op, st->target);
    atlas_screen_push(atlas_screen_install_run(&job));
}

static void activate(menu_state_t *st)
{
    const menu_entry_t *e = &s_entries[st->cursor];

    switch (e->kind) {
    case ENTRY_CARD:
        /* Straight swap between the two slots. An absent card in the
         * other slot is allowed to be selected: the menu then shows it
         * as unusable, which is more informative than a button that
         * appears not to work. */
        st->target = (st->target == ATLAS_DEV_MC0) ? ATLAS_DEV_MC1
                                                   : ATLAS_DEV_MC0;
        refresh_source(st);
        break;

    case ENTRY_EXIT:
        atlas_screen_request_exit();
        break;

    case ENTRY_OP:
    default:
        if (!atlas_install_op_available(e->op, st->target))
            break;

        if (needs_confirmation(e->op))
            st->confirming = 1;
        else
            start(st, e->op);
        break;
    }
}

static void menu_update(atlas_screen_t *self)
{
    menu_state_t *st = (menu_state_t *)self->data;
    u32 rep = atlas_input_repeated();

    if (st->confirming) {
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)) {
            st->confirming = 0;
            start(st, s_entries[st->cursor].op);
        } else if (atlas_input_is_pressed(ATLAS_BTN_BACK)) {
            st->confirming = 0;
        }

        return;
    }

    if (rep & ATLAS_BTN_UP)
        st->cursor = (st->cursor + ENTRY_COUNT - 1) % ENTRY_COUNT;

    if (rep & ATLAS_BTN_DOWN)
        st->cursor = (st->cursor + 1) % ENTRY_COUNT;

    if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM))
        activate(st);

    /* Triangle opens the bootstrap explanation. It is not a menu entry
     * because it is not something this program does - it is the answer
     * to "why will my console not boot the card I just installed to". */
    if (atlas_input_is_pressed(ATLAS_BTN_CONTEXT))
        atlas_screen_push(atlas_ins_screen_bootstrap());

    if (atlas_input_is_pressed(ATLAS_BTN_BACK))
        atlas_screen_request_exit();
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/** One "Label   value" line in the detection block. */
static float info_row(float x, float y, float w, atlas_str_id_t label,
                      const char *value, u64 color)
{
    const atlas_theme_t *t = atlas_theme();

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT, t->text_dim, atlas_str(label));
    atlas_ui_text(x + w, y, ATLAS_ALIGN_RIGHT, color, value);

    return y + atlas_ui_line_height() * 1.15f;
}

static void draw_detected(const menu_state_t *st, float x, float y, float w)
{
    const atlas_theme_t *t = atlas_theme();
    const atlas_device_t *d = atlas_device_get(st->target);
    char buf[96];
    int installed;

    y = info_row(x, y, w, ATLAS_STR_INS_CONSOLE, st->console, t->text);

    /* The slot is named even when empty: "Memory Card 2 - not connected"
     * tells the user which slot to put a card in, where a blank line
     * would not. */
    snprintf(buf, sizeof(buf), "%s", d && d->name ? d->name : "?");
    y = info_row(x, y, w, ATLAS_STR_INS_TARGET, buf,
                 atlas_device_is_ready(st->target) ? t->text : t->warn);

    if (d && d->state == ATLAS_DEV_READY && d->free_kb >= 0)
        snprintf(buf, sizeof(buf), atlas_str(ATLAS_STR_DEV_FREE_KB),
                 d->free_kb);
    else if (d && d->state == ATLAS_DEV_READY)
        snprintf(buf, sizeof(buf), "%s",
                 atlas_str(ATLAS_STR_DEV_FREE_UNKNOWN));
    else
        snprintf(buf, sizeof(buf), "%s",
                 atlas_str(d && d->state == ATLAS_DEV_UNFORMATTED
                           ? ATLAS_STR_DEV_UNFORMATTED
                           : ATLAS_STR_DEV_ABSENT));

    y = info_row(x, y, w, ATLAS_STR_INS_FREE, buf, t->text);

    y = info_row(x, y, w, ATLAS_STR_INS_SOURCE,
                 st->have_source ? st->source
                                 : atlas_str(ATLAS_STR_INS_NONE),
                 st->have_source ? t->text : t->warn);

    installed = atlas_device_is_ready(st->target)
             && atlas_install_is_installed(st->target);

    atlas_ui_text(x, y, ATLAS_ALIGN_LEFT,
                  installed ? t->ok : t->text_dim,
                  atlas_str(installed ? ATLAS_STR_INS_INSTALLED
                                      : ATLAS_STR_INS_NOT_INST));
}

static void menu_draw(atlas_screen_t *self)
{
    menu_state_t *st = (menu_state_t *)self->data;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i;
    char hints[128];

    atlas_ui_header(ATLAS_VERSION_STRING);

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_INS_TITLE));
    y += lh * 2.0f;

    draw_detected(st, x, y, w);
    y += lh * 5.6f;

    atlas_ui_separator(x, y, w, t->separator);
    y += lh * 0.8f;

    for (i = 0; i < ENTRY_COUNT; i++) {
        const menu_entry_t *e = &s_entries[i];
        int usable = e->kind != ENTRY_OP
                  || atlas_install_op_available(e->op, st->target);

        /*
         * An unavailable row still draws, and still draws as selected
         * when the cursor is on it - it just does nothing when pressed.
         * atlas_ui_menu_row already dims an unselected row, so the
         * difference the user sees is that the description below stays
         * blank and the confirm does not fire.
         */
        atlas_ui_menu_row(x, y, w * 0.62f, i == st->cursor,
                          atlas_str(e->label),
                          usable ? NULL : atlas_str(ATLAS_STR_INS_NONE));

        y += (float)(ATLAS_UI_ROW_H + ATLAS_UI_ROW_GAP);
    }

    if (s_entries[st->cursor].detail != ATLAS_STR_COUNT) {
        y += lh * 0.4f;
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(s_entries[st->cursor].detail), w);
    }

    /* Triangle is spelled out as a letter rather than drawn as a glyph:
     * the font atlas covers Latin-1, and a missing character in a hint
     * is worse than a named one. */
    snprintf(hints, sizeof(hints), "X  %s     T  %s     O  %s",
             atlas_str(ATLAS_STR_SELECT),
             atlas_str(ATLAS_STR_INS_BOOT_TITLE),
             atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);

    if (st->confirming) {
        snprintf(hints, sizeof(hints), "X  %s     O  %s",
                 atlas_str(ATLAS_STR_CONFIRM), atlas_str(ATLAS_STR_CANCEL));
        atlas_ui_message_box(atlas_str(s_entries[st->cursor].label),
                             atlas_str(s_entries[st->cursor].detail),
                             hints);
    }
}

static atlas_screen_t s_screen = {
    "InstallerMenu",
    menu_enter,
    NULL,
    menu_update,
    menu_draw,
    &s_state
};

atlas_screen_t *atlas_ins_screen_menu(void)
{
    return &s_screen;
}
