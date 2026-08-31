/*
 * AtlasPS2 - video_cfg.c
 * The parts of the video settings that are just data.
 *
 * Separated from video.c because that file includes gsKit and so can
 * only be built for the EE. These functions are the ones the INI reader
 * and the settings UI need, and keeping them here means the mapping
 * between "pal" in a text file and ATLAS_VMODE_PAL is covered by
 * `make check` rather than only by looking at a television.
 */
#include <string.h>

#include "atlas/video.h"

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

/*
 * Unknown text gives AUTO rather than an error. A configuration naming
 * a mode this build does not have should still produce a picture - and
 * AUTO is the one setting guaranteed to match the console's own region.
 */
atlas_vmode_t atlas_video_mode_from_label(const char *s)
{
    int i;

    if (!s)
        return ATLAS_VMODE_AUTO;

    for (i = 0; i < ATLAS_VMODE_COUNT; i++) {
        if (strcmp(s, atlas_video_mode_label((atlas_vmode_t)i)) == 0)
            return (atlas_vmode_t)i;
    }

    return ATLAS_VMODE_AUTO;
}

atlas_aspect_t atlas_video_aspect_from_label(const char *s)
{
    int i;

    if (!s)
        return ATLAS_ASPECT_AUTO;

    for (i = 0; i < ATLAS_ASPECT_COUNT; i++) {
        if (strcmp(s, atlas_video_aspect_label((atlas_aspect_t)i)) == 0)
            return (atlas_aspect_t)i;
    }

    /* "16/9" and "widescreen" are what people actually type. Accepting
     * them costs two comparisons and saves a support question. */
    if (strcmp(s, "16/9") == 0 || strcmp(s, "widescreen") == 0)
        return ATLAS_ASPECT_16_9;

    if (strcmp(s, "4/3") == 0)
        return ATLAS_ASPECT_4_3;

    return ATLAS_ASPECT_AUTO;
}
