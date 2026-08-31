/*
 * AtlasPS2 - screen.h
 *
 * The screen stack.
 *
 * Every full-screen view - Home, Applications, Settings, a file browser,
 * a modal - is an atlas_screen_t. They form a stack: pushing shows a new
 * screen over the current one, popping returns to what was underneath.
 * That is what makes Back universal, and it means a screen never has to
 * know who opened it or where to return to.
 *
 * Screens are static instances, not allocated: the set is fixed at build
 * time, the depth is bounded, and a menu system that cannot fail to
 * allocate is one less way to strand the user on a black screen.
 */
#ifndef ATLAS_SCREEN_H
#define ATLAS_SCREEN_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atlas_screen atlas_screen_t;

struct atlas_screen {
    const char *name;

    /** Called each time the screen becomes the top of the stack. */
    void (*enter)(atlas_screen_t *self);

    /** Called when it stops being the top, whether pushed over or popped. */
    void (*leave)(atlas_screen_t *self);

    /**
     * One frame of input handling. Called before draw(), with the pad
     * already sampled for this frame.
     */
    void (*update)(atlas_screen_t *self);

    /** Draw one frame. Called between frame_begin and frame_end. */
    void (*draw)(atlas_screen_t *self);

    /** Free for the screen's own state. */
    void *data;
};

/** Maximum stack depth. Deeper than any real navigation path. */
#define ATLAS_SCREEN_STACK_MAX 8

/* ------------------------------------------------------------------ */
/* Stack                                                               */
/* ------------------------------------------------------------------ */

/** Clear the stack and push `root`. */
void atlas_screen_reset(atlas_screen_t *root);

/**
 * Show `screen` over the current one.
 *
 * Pushing past ATLAS_SCREEN_STACK_MAX is ignored rather than fatal: a
 * navigation bug should cost the user one unopened menu, not the
 * session.
 */
void atlas_screen_push(atlas_screen_t *screen);

/** Return to the screen underneath. Ignored at the root. */
void atlas_screen_pop(void);

/** Replace the top screen without growing the stack. */
void atlas_screen_replace(atlas_screen_t *screen);

/** The screen currently on top, or NULL if the stack is empty. */
atlas_screen_t *atlas_screen_top(void);

/** How many screens are stacked. */
int atlas_screen_depth(void);

/**
 * Ask the loop to exit after this frame - used by "launch this ELF" and
 * by the power menu, where the whole environment is going away.
 */
void atlas_screen_request_exit(void);

int atlas_screen_exit_requested(void);

/**
 * Run frames until the stack empties or an exit is requested.
 *
 * Owns the update/draw cycle so no screen has to.
 */
void atlas_screen_run(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_SCREEN_H */
