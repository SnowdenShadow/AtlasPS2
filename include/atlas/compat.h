/*
 * AtlasPS2 - compat.h
 *
 * Per-game settings for booting a disc image: which workarounds a title
 * needs, and what video mode to hand it.
 *
 * WHY A DATABASE AND NOT A HEURISTIC
 * ----------------------------------
 * The workarounds below exist because specific games do specific
 * unusual things - one polls the drive's tray status in a loop, another
 * assumes the disc is a DVD even when it is a CD, a third reads its own
 * executable back and compares it. There is no property of an image
 * that predicts which; the only way to know is that someone tried it
 * and wrote it down.
 *
 * So this is a lookup keyed on the game ID, backed by a text file the
 * user can edit. AtlasPS2 ships no entries of its own: a table baked
 * into the ELF would be a compatibility list that can only be corrected
 * by rebuilding, for a body of knowledge that is collected by players
 * rather than by us.
 *
 * WHAT A WRONG ENTRY COSTS
 * ------------------------
 * A game launched with the wrong settings hangs on a black screen. It
 * does not damage anything - the console is one power cycle away from
 * the menu - but the user has no way to tell a wrong entry from a bad
 * dump. That is why the ID this is keyed on is refused rather than
 * truncated upstream, and why every setting here has an explicit
 * "unset" state distinct from its default.
 */
#ifndef ATLAS_COMPAT_H
#define ATLAS_COMPAT_H

#include "atlas/atlas.h"
#include "atlas/disc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The workaround flags.
 *
 * Each is off unless a game is known to need it. They are bits rather
 * than an enum because a title can need several, and they are named
 * for what they do rather than for the game that first needed them.
 */

/**
 * Report the disc as a DVD even when the image is CD-sized.
 *
 * A handful of titles were mastered on CD but ask the drive for a DVD's
 * layer information and stop if they do not get it.
 */
#define ATLAS_COMPAT_FORCE_DVD      0x0001

/**
 * Never report the tray as having been opened.
 *
 * Some games poll tray status while streaming and treat any change as
 * the disc having been removed. With an emulated drive there is no
 * tray, and the honest answer - "unknown" - is the one they abort on.
 */
#define ATLAS_COMPAT_HIDE_TRAY      0x0002

/**
 * Delay the first read until the game has finished starting up.
 *
 * A few titles issue a read before their own interrupt handlers are
 * installed, on the assumption that a physical drive takes tens of
 * milliseconds to respond. An emulated one answers immediately, and the
 * reply arrives before anything is ready to receive it.
 */
#define ATLAS_COMPAT_SLOW_FIRST_READ 0x0004

/**
 * Leave the IOP's memory map alone when loading modules.
 *
 * The default places our modules high in IOP RAM. A small number of
 * games assume the whole of it is theirs and write over whatever is
 * there, which with an emulated drive is the driver they are reading
 * through.
 */
#define ATLAS_COMPAT_LOW_MODULES    0x0008

/**
 * Do not patch the game's own disc-detection routine.
 *
 * The usual patch makes a game skip its "is this a real disc" check.
 * A few titles use the same routine for something else and misbehave
 * when it is stubbed, so this turns the patch off for them.
 */
#define ATLAS_COMPAT_NO_DISC_PATCH  0x0010

/**
 * What the emulated drive should report as the console's video mode.
 *
 * AUTO means "use the region the disc identified as", which is right
 * for almost everything. The override exists for the discs whose ID
 * prefix does not match how they were mastered, and for a user with a
 * display that only accepts one of the two.
 */
typedef enum {
    ATLAS_VMODE_AUTO = 0,
    ATLAS_VMODE_NTSC,
    ATLAS_VMODE_PAL
} atlas_compat_vmode_t;

typedef struct {
    /** Normalised game ID this applies to, e.g. "SLUS-20902". */
    char id[ATLAS_DISC_ID_MAX];

    /** ATLAS_COMPAT_* bits. */
    u32 flags;

    atlas_compat_vmode_t vmode;
} atlas_compat_t;

/*
 * How many entries are held. A user's own list of the games they have
 * had trouble with is dozens of lines, not thousands; a shared list
 * pasted in from the internet is what would hit this, and the honest
 * response to that is to say so rather than to silently use the first
 * part of it.
 */
#define ATLAS_COMPAT_MAX 256

/**
 * Load the compatibility list from `path`, replacing whatever was held.
 *
 * The file is INI, one section per game:
 *
 *     [SLUS-20902]
 *     force_dvd = 1
 *     vmode = pal
 *
 * A missing file is success with an empty list: no entries means every
 * game launches with defaults, which is the correct behaviour for a
 * fresh installation and for a user who never needed a workaround.
 *
 * @param out_count optional; receives the number of entries loaded.
 * @return ATLAS_OK, or ATLAS_EIO if the file exists but could not be
 *         read.
 */
atlas_err_t atlas_compat_load(const char *path, int *out_count);

/**
 * Look up `id`.
 *
 * @return the entry, or NULL if the game has none - which is the normal
 *         case and means "launch with defaults", not "cannot launch".
 */
const atlas_compat_t *atlas_compat_find(const char *id);

/** How many entries are currently held. */
int atlas_compat_count(void);

/**
 * Parse a settings buffer directly. Exposed for the self-check, which
 * has no filesystem.
 */
atlas_err_t atlas_compat_parse(const char *text, int len, int *out_count);

/** Forget every entry. */
void atlas_compat_clear(void);

/**
 * Decide the video mode to hand a disc, given its region and any
 * per-game override.
 *
 * Kept here rather than at the call site because getting it wrong is
 * invisible until the television is in front of you: a PAL game told it
 * is NTSC runs a fifth too fast with the bottom of the picture cut off,
 * which reads as "this dump is broken" rather than as a setting.
 */
atlas_compat_vmode_t atlas_compat_vmode_for(const atlas_compat_t *entry,
                                            atlas_region_t region);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_COMPAT_H */
