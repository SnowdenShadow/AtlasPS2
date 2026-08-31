/*
 * AtlasPS2 - video.c  (AtlasVideo)
 * GS bring-up, mode handling and the frame cycle.
 */
#include <stdio.h>
#include <string.h>

#include <gsKit.h>
#include <dmaKit.h>

#include "atlas/video.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static GSGLOBAL          *s_gs;
static atlas_video_cfg_t  s_cfg;
static atlas_vmode_t      s_mode;     /* resolved, never AUTO */
static atlas_aspect_t     s_aspect;   /* resolved, never AUTO */
static char               s_name[32];
static int                s_dma_ready;

/*
 * Safe area inset as a fraction of the framebuffer. CRTs overscan by
 * roughly 5% per edge, and a PS2 has no way to query how much a given
 * TV eats, so we keep the UI inside a conservative border and let the
 * user claw pixels back with overscan_x/overscan_y if their set is kind.
 */
#define ATLAS_SAFE_INSET_X_PCT 6
#define ATLAS_SAFE_INSET_Y_PCT 6

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

void atlas_video_cfg_defaults(atlas_video_cfg_t *cfg)
{
    if (!cfg)
        return;

    memset(cfg, 0, sizeof(*cfg));
    cfg->mode   = ATLAS_VMODE_AUTO;
    cfg->aspect = ATLAS_ASPECT_AUTO;
}

const char *atlas_video_mode_label(atlas_vmode_t mode)
{
    switch (mode) {
    case ATLAS_VMODE_AUTO: return "auto";
    case ATLAS_VMODE_NTSC: return "ntsc";
    case ATLAS_VMODE_PAL:  return "pal";
    case ATLAS_VMODE_480P: return "480p";
    default:               return "auto";
    }
}

const char *atlas_video_aspect_label(atlas_aspect_t aspect)
{
    switch (aspect) {
    case ATLAS_ASPECT_AUTO: return "auto";
    case ATLAS_ASPECT_4_3:  return "4:3";
    case ATLAS_ASPECT_16_9: return "16:9";
    default:                return "auto";
    }
}

/* ------------------------------------------------------------------ */
/* Mode resolution                                                     */
/* ------------------------------------------------------------------ */

/*
 * gsKit_check_rom() reads the console ROM version and reports the region
 * default (GS_MODE_NTSC or GS_MODE_PAL). That is the only detection that
 * is safe on every model: probing the GS for an existing signal is not
 * reliable on a cold boot, when nothing has been displayed yet.
 */
static atlas_vmode_t resolve_mode(atlas_vmode_t requested)
{
    if (requested != ATLAS_VMODE_AUTO && requested < ATLAS_VMODE_COUNT)
        return requested;

    return (gsKit_check_rom() == GS_MODE_PAL) ? ATLAS_VMODE_PAL
                                              : ATLAS_VMODE_NTSC;
}

static atlas_aspect_t resolve_aspect(atlas_aspect_t requested)
{
    if (requested == ATLAS_ASPECT_4_3 || requested == ATLAS_ASPECT_16_9)
        return requested;

    /*
     * AUTO stays 4:3. The 16:9 setting of the console lives in the OSD
     * configuration block, whose layout is region dependent; guessing
     * wrong stretches the whole UI, so the safe default is 4:3 and the
     * user picks 16:9 explicitly in Settings.
     */
    return ATLAS_ASPECT_4_3;
}

/** Map our mode to the gsKit GS_MODE_* constant and framebuffer size. */
static void mode_to_gs(atlas_vmode_t mode, s16 *gs_mode, s16 *interlace,
                       s16 *field, int *width, int *height)
{
    switch (mode) {
    case ATLAS_VMODE_PAL:
        *gs_mode   = GS_MODE_PAL;
        *interlace = GS_INTERLACED;
        *field     = GS_FIELD;
        *width     = 640;
        *height    = 512;
        break;

    case ATLAS_VMODE_480P:
        /*
         * 480p needs a component or VGA cable. On a composite/RGB SCART
         * set this produces no picture at all, which is why it is never
         * the AUTO choice and why Recovery forces AUTO back.
         */
        *gs_mode   = GS_MODE_DTV_480P;
        *interlace = GS_NONINTERLACED;
        *field     = GS_FRAME;
        *width     = 640;
        *height    = 448;
        break;

    case ATLAS_VMODE_NTSC:
    default:
        *gs_mode   = GS_MODE_NTSC;
        *interlace = GS_INTERLACED;
        *field     = GS_FIELD;
        *width     = 640;
        *height    = 448;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Screen setup                                                        */
/* ------------------------------------------------------------------ */

static void open_screen(void)
{
    s16 gs_mode, interlace, field;
    int width, height;

    mode_to_gs(s_mode, &gs_mode, &interlace, &field, &width, &height);

    s_gs->Mode       = gs_mode;
    s_gs->Interlace  = interlace;
    s_gs->Field      = field;
    s_gs->Width      = width;
    s_gs->Height     = height;

    /* CT24: no alpha in the framebuffer, so it costs 3/4 of what a CT32
     * buffer would take in the 4 MB of GS VRAM. Per-primitive alpha
     * blending is unaffected - that happens before the write. */
    s_gs->PSM        = GS_PSM_CT24;
    s_gs->PSMZ       = GS_PSMZ_16S;
    s_gs->ZBuffering = GS_SETTING_OFF; /* pure 2D UI, no depth needed */
    s_gs->DoubleBuffering = GS_SETTING_ON;
    s_gs->PrimAlphaEnable = GS_SETTING_ON;
    s_gs->Dithering       = GS_SETTING_ON;

    /* gsKit derives MAGH/MAGV and DW/DH from Width/Height for us. */
    gsKit_init_screen(s_gs);

    /* User trim for TVs whose picture sits off-centre. */
    if (s_cfg.offset_x || s_cfg.offset_y) {
        gsKit_set_display_offset(s_gs,
                                 ATLAS_CLAMP(s_cfg.offset_x, -64, 64),
                                 ATLAS_CLAMP(s_cfg.offset_y, -64, 64));
    }

    gsKit_set_test(s_gs, GS_ZTEST_OFF);
    gsKit_set_primalpha(s_gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gsKit_mode_switch(s_gs, GS_ONESHOT);

    snprintf(s_name, sizeof(s_name), "%s %dx%d",
             s_mode == ATLAS_VMODE_PAL  ? "PAL"  :
             s_mode == ATLAS_VMODE_480P ? "480p" : "NTSC",
             width, height);

    ATLAS_LOG("VIDEO", "screen %s aspect %s", s_name,
              atlas_video_aspect_label(s_aspect));
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_video_init(const atlas_video_cfg_t *cfg)
{
    if (s_gs)
        return atlas_video_apply(cfg);

    if (cfg)
        s_cfg = *cfg;
    else
        atlas_video_cfg_defaults(&s_cfg);

    s_mode   = resolve_mode(s_cfg.mode);
    s_aspect = resolve_aspect(s_cfg.aspect);

    if (!s_dma_ready) {
        /*
         * GIF is the only channel AtlasPS2 drives. Requesting just that
         * one keeps the DMA controller setup minimal.
         */
        dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                    D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
        dmaKit_chan_init(DMA_CHANNEL_GIF);
        s_dma_ready = 1;
    }

    s_gs = gsKit_init_global();
    if (!s_gs) {
        ATLAS_LOG("VIDEO", "gsKit_init_global failed");
        return ATLAS_EFATAL;
    }

    open_screen();
    return ATLAS_OK;
}

atlas_err_t atlas_video_apply(const atlas_video_cfg_t *cfg)
{
    if (!s_gs)
        return atlas_video_init(cfg);

    if (cfg)
        s_cfg = *cfg;

    s_mode   = resolve_mode(s_cfg.mode);
    s_aspect = resolve_aspect(s_cfg.aspect);

    open_screen();

    return ATLAS_OK;
}

void atlas_video_shutdown(void)
{
    if (!s_gs)
        return;

    gsKit_deinit_global(s_gs);
    s_gs = NULL;
    ATLAS_LOG("VIDEO", "shutdown");
}

int atlas_video_ready(void)
{
    return s_gs != NULL;
}

/* ------------------------------------------------------------------ */
/* Frame cycle                                                         */
/* ------------------------------------------------------------------ */

void atlas_video_frame_begin(u64 clear_color)
{
    if (!s_gs)
        return;

    gsKit_clear(s_gs, clear_color);
}

void atlas_video_frame_end(void)
{
    if (!s_gs)
        return;

    /*
     * Order matters: push the queue to the GS, then wait for vsync and
     * flip. Doing it the other way round shows a half-drawn frame.
     */
    gsKit_queue_exec(s_gs);
    gsKit_sync_flip(s_gs);
}

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */

int atlas_video_width(void)
{
    return s_gs ? s_gs->Width : 640;
}

int atlas_video_height(void)
{
    return s_gs ? s_gs->Height : 448;
}

int atlas_video_safe_x(void)
{
    int w = atlas_video_width();
    return (w * ATLAS_SAFE_INSET_X_PCT) / 100
           + ATLAS_CLAMP(s_cfg.overscan_x, 0, 64);
}

int atlas_video_safe_y(void)
{
    int h = atlas_video_height();
    return (h * ATLAS_SAFE_INSET_Y_PCT) / 100
           + ATLAS_CLAMP(s_cfg.overscan_y, 0, 64);
}

int atlas_video_safe_w(void)
{
    return atlas_video_width() - 2 * atlas_video_safe_x();
}

int atlas_video_safe_h(void)
{
    return atlas_video_height() - 2 * atlas_video_safe_y();
}

atlas_vmode_t atlas_video_mode(void)
{
    return s_mode;
}

atlas_aspect_t atlas_video_aspect(void)
{
    return s_aspect;
}

const char *atlas_video_mode_name(void)
{
    return s_name[0] ? s_name : "unknown";
}

float atlas_video_x_scale(void)
{
    /*
     * A 16:9 TV stretches the same 640-pixel-wide buffer over a wider
     * picture. Narrowing the UI horizontally by 4:3 / 16:9 = 0.75 keeps
     * circles round and panels square once the TV does the stretching.
     */
    return (s_aspect == ATLAS_ASPECT_16_9) ? 0.75f : 1.0f;
}

GSGLOBAL *atlas_video_gs(void)
{
    return s_gs;
}
