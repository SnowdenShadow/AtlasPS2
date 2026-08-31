/*
 * AtlasPS2 - video.h  (AtlasVideo)
 *
 * Owns the GS: mode selection, framebuffer setup, screen geometry and the
 * per-frame begin/end cycle. Everything else in AtlasPS2 draws through
 * this module and never touches gsKit's global state directly.
 *
 * HARDWARE NOTE ON RESOLUTION
 * ---------------------------
 * The PS2 renders into a framebuffer in GS VRAM (4 MB total) and the GS
 * then scans that buffer out with the video timings of the selected mode.
 * Selecting a 480p/1080i output mode changes the OUTPUT TIMING only, it
 * does NOT make anything render at that pixel count: our UI framebuffer
 * stays 640x448 (NTSC) or 640x512 (PAL). Anything claiming a PS2 "runs at
 * 1080p" is describing the output signal, not the rendering resolution.
 */
#ifndef ATLAS_VIDEO_H
#define ATLAS_VIDEO_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration so callers do not need gsKit headers unless they
 * actually draw. atlas_video_gs() hands out the real pointer. */
struct gsGlobal;

/* ------------------------------------------------------------------ */
/* Modes                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    ATLAS_VMODE_AUTO = 0, /* follow the console ROM region (safe default) */
    ATLAS_VMODE_NTSC,     /* 640x448 interlaced, 59.94 Hz                 */
    ATLAS_VMODE_PAL,      /* 640x512 interlaced, 50 Hz                    */
    ATLAS_VMODE_480P,     /* 640x448 progressive - needs component/VGA    */
    ATLAS_VMODE_COUNT
} atlas_vmode_t;

typedef enum {
    ATLAS_ASPECT_AUTO = 0, /* 4:3 unless the console is configured 16:9 */
    ATLAS_ASPECT_4_3,
    ATLAS_ASPECT_16_9,
    ATLAS_ASPECT_COUNT
} atlas_aspect_t;

/** Video settings. Mirrors the [video] section of ATLAS.INI. */
typedef struct {
    atlas_vmode_t  mode;
    atlas_aspect_t aspect;
    int            offset_x;  /* screen position trim, pixels, -32..32 */
    int            offset_y;
    int            overscan_x; /* extra safe-area inset, pixels, 0..64 */
    int            overscan_y;
} atlas_video_cfg_t;

/** Fill cfg with the safe defaults (AUTO/AUTO, no offsets). */
void atlas_video_cfg_defaults(atlas_video_cfg_t *cfg);

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * Bring up DMA + the GS and open the screen.
 *
 * Passing NULL uses the safe defaults, which is what Recovery does: it
 * must come up even when the stored configuration is what broke video.
 *
 * @return ATLAS_OK, or ATLAS_EFATAL if the GS could not be initialised
 *         (there is no way to show a UI after that).
 */
atlas_err_t atlas_video_init(const atlas_video_cfg_t *cfg);

/** Re-open the screen with new settings. Safe to call at runtime. */
atlas_err_t atlas_video_apply(const atlas_video_cfg_t *cfg);

/**
 * Change only the position and margin, without re-opening the screen.
 *
 * These four are the settings a user adjusts by eye, one press at a
 * time, watching the picture move. Routing them through
 * atlas_video_apply() would cost a full mode switch per press - a black
 * frame and a television re-syncing - which makes the thing being
 * adjusted impossible to see. The mode and the aspect are untouched.
 */
void atlas_video_set_trim(int offset_x, int offset_y,
                          int overscan_x, int overscan_y);

/** Release the GS. Call before launching another ELF. */
void atlas_video_shutdown(void);

/** Non-zero once atlas_video_init() has succeeded. */
int atlas_video_ready(void);

/**
 * The settings the screen is currently open with.
 *
 * Not the same thing as the [video] block of ATLAS.INI: holding R1 at
 * boot skips the stored settings entirely, and the settings screen
 * applies changes live before anything is written. A revert has to
 * restore what is actually on the television, so it reads this.
 *
 * Still holds AUTO where the user chose AUTO - use atlas_video_mode()
 * for what AUTO resolved to.
 */
const atlas_video_cfg_t *atlas_video_cfg(void);

/* ------------------------------------------------------------------ */
/* Frame cycle                                                         */
/* ------------------------------------------------------------------ */

/** Start a frame: clears the back buffer to the given colour. */
void atlas_video_frame_begin(u64 clear_color);

/** Finish a frame: flush the draw queue, wait for vsync, flip. */
void atlas_video_frame_end(void);

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/*                                                                     */
/* UI code lays out against the SAFE AREA, not the framebuffer, so the */
/* same layout survives CRT overscan on both PAL and NTSC.             */
/* ------------------------------------------------------------------ */

int atlas_video_width(void);      /* framebuffer width  (640)          */
int atlas_video_height(void);     /* framebuffer height (448 or 512)   */

int atlas_video_safe_x(void);     /* safe area origin and size         */
int atlas_video_safe_y(void);
int atlas_video_safe_w(void);
int atlas_video_safe_h(void);

/** Currently active mode, resolved (never returns ATLAS_VMODE_AUTO). */
atlas_vmode_t atlas_video_mode(void);

/** Currently active aspect, resolved (never returns ATLAS_ASPECT_AUTO). */
atlas_aspect_t atlas_video_aspect(void);

/** Short label for the UI, e.g. "PAL 640x512". Never NULL. */
const char *atlas_video_mode_name(void);

/**
 * Horizontal scale factor applied to UI x-coordinates so a layout
 * authored for 4:3 keeps its proportions when the user selects 16:9.
 */
float atlas_video_x_scale(void);

/** The live gsKit context, or NULL before init. Renderer use only. */
struct gsGlobal *atlas_video_gs(void);

/* Names for the settings UI and the INI parser. */
const char *atlas_video_mode_label(atlas_vmode_t mode);
const char *atlas_video_aspect_label(atlas_aspect_t aspect);

/**
 * The reverse: text from ATLAS.INI to a setting.
 *
 * Unknown text gives AUTO rather than failing. A configuration naming a
 * mode this build does not have should still produce a picture, and
 * AUTO is the one value guaranteed to match the console's own region.
 */
atlas_vmode_t  atlas_video_mode_from_label(const char *s);
atlas_aspect_t atlas_video_aspect_from_label(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_VIDEO_H */
