/*
 * AtlasPS2 - screen.c
 * The screen stack and the main loop.
 */
#include <string.h>

#include "atlas/screen.h"
#include "atlas/video.h"
#include "atlas/input.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/device.h"
#include "atlas/i18n.h"
#include "atlas/log.h"

static atlas_screen_t *s_stack[ATLAS_SCREEN_STACK_MAX];
static int s_depth;
static int s_exit;

/*
 * A push or pop taking effect in the middle of a frame would leave the
 * new screen drawing before its enter() had run, so transitions are
 * recorded here and applied between frames instead.
 */
typedef enum {
    PENDING_NONE = 0,
    PENDING_PUSH,
    PENDING_POP,
    PENDING_REPLACE
} pending_kind_t;

static pending_kind_t s_pending;
static atlas_screen_t *s_pending_screen;

/* ------------------------------------------------------------------ */
/* Stack operations                                                    */
/* ------------------------------------------------------------------ */

static void enter_screen(atlas_screen_t *s)
{
    if (s && s->enter)
        s->enter(s);
}

static void leave_screen(atlas_screen_t *s)
{
    if (s && s->leave)
        s->leave(s);
}

void atlas_screen_reset(atlas_screen_t *root)
{
    int i;

    for (i = s_depth - 1; i >= 0; i--)
        leave_screen(s_stack[i]);

    s_depth = 0;
    s_exit = 0;
    s_pending = PENDING_NONE;
    s_pending_screen = NULL;

    if (root) {
        s_stack[0] = root;
        s_depth = 1;
        enter_screen(root);
    }
}

void atlas_screen_push(atlas_screen_t *screen)
{
    if (!screen)
        return;

    if (s_depth >= ATLAS_SCREEN_STACK_MAX) {
        ATLAS_LOG("UI", "screen stack full, ignoring push of %s",
                  screen->name ? screen->name : "?");
        return;
    }

    s_pending = PENDING_PUSH;
    s_pending_screen = screen;
}

void atlas_screen_pop(void)
{
    /* The root has nowhere to go back to; Back there is a no-op. */
    if (s_depth <= 1)
        return;

    s_pending = PENDING_POP;
    s_pending_screen = NULL;
}

void atlas_screen_replace(atlas_screen_t *screen)
{
    if (!screen)
        return;

    s_pending = PENDING_REPLACE;
    s_pending_screen = screen;
}

static void apply_pending(void)
{
    switch (s_pending) {
    case PENDING_PUSH:
        leave_screen(s_stack[s_depth - 1]);
        s_stack[s_depth++] = s_pending_screen;
        enter_screen(s_pending_screen);
        break;

    case PENDING_POP:
        leave_screen(s_stack[--s_depth]);
        enter_screen(s_stack[s_depth - 1]);
        break;

    case PENDING_REPLACE:
        if (s_depth > 0) {
            leave_screen(s_stack[s_depth - 1]);
            s_stack[s_depth - 1] = s_pending_screen;
        } else {
            s_stack[s_depth++] = s_pending_screen;
        }
        enter_screen(s_pending_screen);
        break;

    case PENDING_NONE:
        return;
    }

    s_pending = PENDING_NONE;
    s_pending_screen = NULL;
}

atlas_screen_t *atlas_screen_top(void)
{
    return s_depth > 0 ? s_stack[s_depth - 1] : NULL;
}

int atlas_screen_depth(void)
{
    return s_depth;
}

void atlas_screen_request_exit(void)
{
    s_exit = 1;
}

int atlas_screen_exit_requested(void)
{
    return s_exit;
}

/* ------------------------------------------------------------------ */
/* Main loop                                                           */
/* ------------------------------------------------------------------ */

/*
 * How long an interface may go without ever hearing from a controller
 * before it says so, in frames. About two seconds, which is longer than
 * a pad plugged in at power-on takes to answer and short enough that a
 * user who is pressing buttons at nothing does not sit there wondering.
 *
 * The condition is "has NEVER answered", not "is not answering now": a
 * controller unplugged mid-session is a different situation, the user
 * knows what they just did, and covering the screen for it would be
 * noise.
 */
#define NO_PAD_GRACE_FRAMES 120

/**
 * Say that nothing is plugged in, over whatever screen is up.
 *
 * Drawn here rather than in each screen because it applies to all of
 * them, and because the screen it matters most on is the first-boot
 * wizard - the one screen with no way out and no Back.
 */
static void draw_no_pad(void)
{
    atlas_ui_message_box(atlas_str(ATLAS_STR_PAD_NONE_TITLE),
                         atlas_str(ATLAS_STR_PAD_NONE_BODY),
                         NULL);
}

void atlas_screen_run(void)
{
    int no_pad_frames = 0;

    while (!s_exit && s_depth > 0) {
        atlas_screen_t *top = s_stack[s_depth - 1];

        atlas_input_update();

        /*
         * One device is probed per frame, so a full sweep costs four
         * frames and no single frame pays for all four. Done here
         * rather than in each screen because the header indicators are
         * drawn everywhere, and draw must never block on hardware.
         */
        atlas_device_poll();

        if (top->update)
            top->update(top);

        /*
         * A screen that requested an exit or a transition during
         * update() still gets drawn this frame - dropping the frame
         * would show one blank flash on every menu change.
         */
        atlas_video_frame_begin(atlas_theme()->bg_top);

        atlas_ui_background();

        if (top->draw)
            top->draw(top);

        if (!atlas_input_ever_connected()) {
            if (no_pad_frames < NO_PAD_GRACE_FRAMES)
                no_pad_frames++;
            else
                draw_no_pad();
        }

        atlas_video_frame_end();

        apply_pending();
    }
}
