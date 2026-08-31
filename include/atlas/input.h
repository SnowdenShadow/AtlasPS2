/*
 * AtlasPS2 - input.h
 *
 * Controller handling. Wraps libpad and turns the raw per-frame button
 * mask into the edge-triggered events a menu actually wants, plus a
 * key-repeat for held directions.
 *
 * REGION NOTE
 * -----------
 * The physical buttons are PAD_CROSS and PAD_CIRCLE, but their MEANING
 * differs: Japanese systems traditionally confirm with Circle, Western
 * ones with Cross. AtlasPS2 never hardcodes "cross == confirm" in UI
 * code - it asks for ATLAS_BTN_CONFIRM and this module maps that onto
 * the right physical button for the configured layout.
 */
#ifndef ATLAS_INPUT_H
#define ATLAS_INPUT_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Logical buttons                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    ATLAS_BTN_UP       = 1 << 0,
    ATLAS_BTN_DOWN     = 1 << 1,
    ATLAS_BTN_LEFT     = 1 << 2,
    ATLAS_BTN_RIGHT    = 1 << 3,
    ATLAS_BTN_CONFIRM  = 1 << 4,  /* Cross  (or Circle on JP layout) */
    ATLAS_BTN_BACK     = 1 << 5,  /* Circle (or Cross  on JP layout) */
    ATLAS_BTN_CONTEXT  = 1 << 6,  /* Triangle */
    ATLAS_BTN_ACTION   = 1 << 7,  /* Square   */
    ATLAS_BTN_PREV_TAB = 1 << 8,  /* L1 */
    ATLAS_BTN_NEXT_TAB = 1 << 9,  /* R1 */
    ATLAS_BTN_L2       = 1 << 10,
    ATLAS_BTN_R2       = 1 << 11,
    ATLAS_BTN_START    = 1 << 12,
    ATLAS_BTN_SELECT   = 1 << 13
} atlas_btn_t;

/** Which physical button confirms. */
typedef enum {
    ATLAS_LAYOUT_CROSS_CONFIRM = 0, /* Western default */
    ATLAS_LAYOUT_CIRCLE_CONFIRM     /* Japanese layout */
} atlas_pad_layout_t;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * Load the pad IOP modules and open both controller ports.
 *
 * Returns ATLAS_OK if at least one port opened. A pad that is simply not
 * plugged in yet is not an error: atlas_input_update() keeps polling and
 * picks it up when the user connects it.
 */
atlas_err_t atlas_input_init(void);

/** Close the pad ports. Call before launching another ELF. */
void atlas_input_shutdown(void);

/** Choose which physical button means confirm. */
void atlas_input_set_layout(atlas_pad_layout_t layout);

atlas_pad_layout_t atlas_input_layout(void);

/* ------------------------------------------------------------------ */
/* Per-frame polling                                                   */
/* ------------------------------------------------------------------ */

/**
 * Sample the controller. Call exactly once per frame, before reading any
 * of the query functions below.
 *
 * Safe to call when no pad is connected - everything simply reads as
 * "nothing pressed", and a controller unplugged mid-session does not
 * wedge the UI.
 */
void atlas_input_update(void);

/** Buttons held down this frame (bitmask of atlas_btn_t). */
u32 atlas_input_held(void);

/** Buttons that went down between the previous frame and this one. */
u32 atlas_input_pressed(void);

/** Buttons that were released between the previous frame and this one. */
u32 atlas_input_released(void);

/**
 * Like atlas_input_pressed(), but a held direction auto-repeats after a
 * short delay, so holding Down scrolls a long list instead of stepping
 * once. Menus should use this for navigation and plain pressed() for
 * confirm/back, where a repeat would double-fire.
 */
u32 atlas_input_repeated(void);

/** Convenience wrappers around the masks above. */
int atlas_input_is_held(atlas_btn_t btn);
int atlas_input_is_pressed(atlas_btn_t btn);

/** Non-zero while a controller is connected and reporting. */
int atlas_input_connected(void);

/**
 * Raw libpad button mask for port 0, already inverted so a set bit means
 * "pressed". Used by boot-time hotkey detection, which runs before the
 * logical mapping exists.
 */
u16 atlas_input_raw(void);

/** Left analog stick, -128..127, 0 at rest. Zero when no analog pad. */
int atlas_input_stick_x(void);
int atlas_input_stick_y(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_INPUT_H */
