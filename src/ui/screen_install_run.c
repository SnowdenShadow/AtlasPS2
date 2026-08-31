/*
 * AtlasPS2 - screen_install_run.c
 * The progress screen: five lines, each one gaining a tick as it lands.
 *
 * WHY THE ENGINE DRAWS THROUGH THIS SCREEN
 * ----------------------------------------
 * A verified copy of a 700 KB ELF to a Memory Card is three passes over
 * the file and takes long enough that a still picture reads as a hang.
 * The user's reflex then is to pull the card - during the one operation
 * on it that must not be interrupted. So the engine calls back into
 * here mid-step and this screen renders a frame, which means the bar
 * moves while a step is running rather than only between steps.
 *
 * Shared by the installer and by Recovery. Both watch the same engine
 * perform the same five steps, and a second copy of this screen would
 * be a second place for the "do not remove the card" line to be missing
 * from.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/screens.h"
#include "atlas/install.h"

#include "atlas/i18n.h"
#include "atlas/input.h"
#include "atlas/theme.h"
#include "atlas/ui.h"
#include "atlas/video.h"

typedef struct {
    atlas_install_job_t job;

    /* Progress inside the running step. Reset per step so the bar
     * measures the step, not the whole operation: the steps are wildly
     * different lengths and one bar over all of them would sit still
     * for the copy and then jump. */
    int done;
    int total;

    /* Frames to wait before running the next step. The steps that touch
     * a card take far longer than a frame, but the ones that do not
     * would otherwise all complete in the same frame and the list would
     * appear finished before the user could read it. */
    int delay;
} run_state_t;

static run_state_t s_state;

#define STEP_DELAY_FRAMES 12   /* ~0.2 s: readable, not sluggish */

static void draw_frame(void);

/* ------------------------------------------------------------------ */
/* Progress bridge                                                     */
/* ------------------------------------------------------------------ */

/**
 * Called from inside a long file operation.
 *
 * Draws a whole frame from within the engine's call stack. That is
 * unusual, and it is deliberate: the alternative is a screen that
 * cannot update until the operation it is reporting on has finished.
 */
static void on_tick(const atlas_install_job_t *job, int done, int total,
                    void *ctx)
{
    (void)job;
    (void)ctx;

    s_state.done = done;
    s_state.total = total;

    atlas_video_frame_begin(atlas_theme()->bg_top);
    atlas_ui_background();
    draw_frame();
    atlas_video_frame_end();
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static void run_enter(atlas_screen_t *self)
{
    run_state_t *st = (run_state_t *)self->data;

    st->done = 0;
    st->total = -1;
    st->delay = STEP_DELAY_FRAMES;

    atlas_install_set_tick(on_tick, NULL);
}

static void run_leave(atlas_screen_t *self)
{
    (void)self;

    /* The engine outlives this screen; leaving a callback pointing at a
     * screen that is no longer on the stack would draw over whatever
     * replaced it. */
    atlas_install_set_tick(NULL, NULL);
}

static void run_update(atlas_screen_t *self)
{
    run_state_t *st = (run_state_t *)self->data;

    if (st->job.done) {
        /*
         * Only Back leaves, and only once it is over. There is no
         * cancel: every step either completes or rolls itself back, and
         * a button that abandoned a half-finished swap would be the one
         * way to break a card from inside this program.
         */
        if (atlas_input_is_pressed(ATLAS_BTN_CONFIRM)
            || atlas_input_is_pressed(ATLAS_BTN_BACK))
            atlas_screen_pop();

        return;
    }

    if (st->delay > 0) {
        st->delay--;
        return;
    }

    st->done = 0;
    st->total = -1;

    atlas_install_pump(&st->job);

    st->delay = STEP_DELAY_FRAMES;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/** The tick, cross or dash at the end of a step line. */
static const char *state_mark(atlas_install_step_state_t s)
{
    switch (s) {
    case ATLAS_STEP_DONE:    return "OK";
    case ATLAS_STEP_FAILED:  return "X";
    case ATLAS_STEP_RUNNING: return "...";
    case ATLAS_STEP_SKIPPED: return "-";
    default:                 return "";
    }
}

static u64 state_color(atlas_install_step_state_t s)
{
    const atlas_theme_t *t = atlas_theme();

    switch (s) {
    case ATLAS_STEP_DONE:    return t->ok;
    case ATLAS_STEP_FAILED:  return t->error;
    case ATLAS_STEP_RUNNING: return t->accent;
    default:                 return t->text_dim;
    }
}

/**
 * The bar under the running step.
 *
 * Drawn only while a step is actually reporting bytes. A bar that sat
 * at zero during the steps with nothing to measure would look stuck at
 * exactly the moment the program is asking to be trusted.
 */
static void draw_bar(float x, float y, float w, int done, int total)
{
    const atlas_theme_t *t = atlas_theme();
    float frac;

    if (total <= 0 || done <= 0)
        return;

    frac = (float)done / (float)total;
    if (frac > 1.0f)
        frac = 1.0f;

    atlas_ui_rect(x, y, w, 6.0f, t->panel);
    atlas_ui_rect(x, y, w * frac, 6.0f, t->accent);
}

static void draw_frame(void)
{
    run_state_t *st = &s_state;
    const atlas_theme_t *t = atlas_theme();
    float sw = (float)atlas_video_safe_w() / atlas_video_x_scale();
    float x = (float)ATLAS_UI_PAD;
    float w = sw - (float)ATLAS_UI_PAD * 2.0f;
    float lh = atlas_ui_line_height();
    float y;
    int i;
    char hints[64];

    atlas_ui_header(ATLAS_VERSION_STRING);

    y = (float)ATLAS_UI_HEADER_H + (float)ATLAS_UI_PAD;
    atlas_ui_text_title(x, y, ATLAS_ALIGN_LEFT, t->text,
                        atlas_str(ATLAS_STR_INS_TITLE));
    y += lh * 2.2f;

    for (i = 0; i < ATLAS_STEP_COUNT; i++) {
        atlas_install_step_state_t s = st->job.state[i];
        u64 color = state_color(s);

        atlas_ui_text(x, y, ATLAS_ALIGN_LEFT,
                      s == ATLAS_STEP_SKIPPED ? t->text_dim : t->text,
                      atlas_str(atlas_install_step_label(
                          (atlas_install_step_t)i)));

        atlas_ui_text(x + w, y, ATLAS_ALIGN_RIGHT, color, state_mark(s));

        y += lh * 1.2f;

        if (s == ATLAS_STEP_RUNNING) {
            draw_bar(x, y - lh * 0.2f, w, st->done, st->total);
            y += lh * 0.5f;
        }
    }

    y += lh * 0.8f;

    if (!st->job.done) {
        /* The one instruction that matters while this is running. */
        atlas_ui_text_clipped(x, y, t->warn,
                              atlas_str(ATLAS_STR_INS_WORKING), w);
        atlas_ui_footer("");
        return;
    }

    atlas_ui_text_clipped(x, y,
                          st->job.err == ATLAS_OK ? t->ok : t->error,
                          atlas_str(st->job.message), w);

    if (st->job.err == ATLAS_OK
        && (st->job.op == ATLAS_OP_INSTALL || st->job.op == ATLAS_OP_UPDATE)) {
        y += lh * 1.4f;
        atlas_ui_text_clipped(x, y, t->text_dim,
                              atlas_str(ATLAS_STR_INS_RESTART), w);
    }

    snprintf(hints, sizeof(hints), "O  %s", atlas_str(ATLAS_STR_BACK));
    atlas_ui_footer(hints);
}

static void run_draw(atlas_screen_t *self)
{
    (void)self;
    draw_frame();
}

static atlas_screen_t s_screen = {
    "InstallRun",
    run_enter,
    run_leave,
    run_update,
    run_draw,
    &s_state
};

atlas_screen_t *atlas_screen_install_run(const atlas_install_job_t *job)
{
    if (job)
        memcpy(&s_state.job, job, sizeof(s_state.job));

    return &s_screen;
}
