/*
 * AtlasPS2 - profile.h
 *
 * Per-title display settings: one small INI file per game, named after
 * its ID, holding how AtlasPS2 should set the screen up before handing
 * the console over.
 *
 * WHY THIS IS NOT compat.h
 * ------------------------
 * They look alike and are deliberately separate. A compatibility entry
 * describes what the emulated drive must lie about for a title to boot
 * at all; a profile describes how the user wants that title presented,
 * and is a preference. The two have different authors - the first is
 * knowledge collected by players and pasted between launchers, the
 * second is one person's television - and merging them would mean
 * importing somebody's shared compatibility list overwrote the offsets
 * that person had trimmed for their own set.
 *
 * One file per game rather than one file with a section per game, for
 * the same reason: a profile is written by the program when the user
 * changes it, and rewriting a shared file to change one game's offset
 * puts every other game's settings at risk of a bad write. A file that
 * fails to write costs one title's preferences.
 *
 * WHAT A PROFILE MAY NOT DO
 * -------------------------
 * Nothing here patches a game. The spec is explicit that questionable
 * patches must not be applied blindly, and the honest boundary is that
 * a profile only sets things AtlasPS2 already lets a user set for
 * itself: the video mode, the aspect, the screen position, and which
 * application to hand the title to. Every field has an explicit unset
 * state, so a profile that mentions one setting leaves the rest alone
 * rather than silently applying defaults the user never chose.
 *
 * PROFILES ARE OPTIONAL
 * ---------------------
 * No profile is the normal case. A title with none launches exactly as
 * it would if this module did not exist, which is what makes it safe
 * for the feature to be absent, empty, or unreadable.
 */
#ifndef ATLAS_PROFILE_H
#define ATLAS_PROFILE_H

#include "atlas/atlas.h"
#include "atlas/disc.h"
#include "atlas/video.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A field that was not mentioned in the file.
 *
 * Distinct from every real value on purpose. "Unset" and "zero" are
 * different answers for an offset: the first means "leave whatever the
 * user set globally", the second means "centre it". Collapsing them
 * would make a profile that only sets the aspect quietly reset the
 * screen trim someone spent ten minutes on.
 */
#define ATLAS_PROFILE_UNSET (-1000)

/** How long a launch_app path may be. Matches the app catalogue. */
#define ATLAS_PROFILE_PATH_MAX 128

typedef struct {
    /** The game this belongs to, normalised, e.g. "SLUS-20946". */
    char id[ATLAS_DISC_ID_MAX];

    /**
     * Video mode to switch to before launching, or ATLAS_VMODE_COUNT
     * for "leave it alone".
     *
     * Named as the video module names them, not as the compatibility
     * database does: this one really does change what the GS outputs,
     * while a compatibility vmode only changes what the emulated drive
     * tells the game.
     */
    atlas_vmode_t mode;

    /** Aspect, or ATLAS_ASPECT_COUNT for "leave it alone". */
    atlas_aspect_t aspect;

    /** Screen trim, or ATLAS_PROFILE_UNSET. Clamped as [video] is. */
    int offset_x;
    int offset_y;

    /**
     * The title is a widescreen release, or ATLAS_PROFILE_UNSET.
     *
     * Recorded, not acted on. AtlasPS2 does not patch games, and a
     * game's internal rendering aspect is a property of the game: the
     * flag exists so a profile can carry what the user knows about the
     * title, and so the launcher it is handed to can be told. Setting
     * it changes nothing about how AtlasPS2 itself draws.
     */
    int widescreen;

    /**
     * Path of the program to hand this title to, or "" for the
     * default.
     *
     * The field the whole file is worth having for: a user with two
     * launchers installed knows which one runs which game, and this is
     * where that knowledge goes. Not checked here - a path that no
     * longer resolves is reported at launch time, where there is a
     * screen to report it on.
     */
    char launch_app[ATLAS_PROFILE_PATH_MAX];
} atlas_profile_t;

/**
 * Fill `p` with every field unset, and no ID.
 *
 * This - not a zeroed struct - is what "no profile" means, and callers
 * that read a missing file get exactly this.
 */
void atlas_profile_defaults(atlas_profile_t *p);

/**
 * Whether `p` actually asks for anything.
 *
 * @return 0 when every field is unset, which a file full of unknown
 *         keys also produces. The caller uses it to decide whether to
 *         say a profile is in effect.
 */
int atlas_profile_is_empty(const atlas_profile_t *p);

/**
 * Parse one profile file over `p`.
 *
 * Keys, all optional:
 *
 *     video_mode  = auto | ntsc | pal | 480p
 *     aspect_ratio = auto | 4:3 | 16:9
 *     offset_x    = -32..32
 *     offset_y    = -32..32
 *     widescreen  = yes | no
 *     launch_app  = mc0:/APPS/OPL.ELF
 *
 * Section headers are ignored: the file is named after the game, so a
 * `[SLUS-20946]` header inside it is redundant, and a user who writes
 * one should not have their file silently do nothing.
 *
 * An unreadable value leaves that field alone and is logged. A file
 * whose every line is bad parses to an empty profile, which behaves
 * exactly like no profile - the safe direction.
 *
 * @return ATLAS_OK, or ATLAS_EINVAL for a NULL argument.
 */
atlas_err_t atlas_profile_parse(atlas_profile_t *p, const char *text, int len);

/**
 * Render `p` as the text of a profile file.
 *
 * Unset fields are omitted rather than written as "auto": a file that
 * lists every key looks like a set of deliberate choices, and the next
 * person to read it - possibly the same person a year later - cannot
 * tell which ones were.
 *
 * @return bytes written, or -1 if `size` was too small.
 */
int atlas_profile_format(const atlas_profile_t *p, char *out, int size);

/**
 * Build the path a title's profile lives at.
 *
 * `dir` is the profiles folder, e.g. "mc0:/ATLAS/PROFILES". The file
 * name is the game ID with its dash removed and `.INI` appended, so
 * "SLUS-20946" becomes "SLUS_20946.INI": Memory Card filesystems are
 * 8.3-ish and a dash is not the safest character to rely on there.
 *
 * @return ATLAS_OK, ATLAS_EINVAL, or ATLAS_EFORMAT if `id` is not a
 *         game ID.
 */
atlas_err_t atlas_profile_path(const char *dir, const char *id,
                               char *out, int size);

/** The longest profile file this module will read or write. */
#define ATLAS_PROFILE_FILE_MAX 512

/**
 * Read the profile for `id` from `dir`.
 *
 * A missing file is success with an empty profile: no profile is the
 * normal case, and an error for it would put a warning on screen for
 * every game that never needed one.
 *
 * @return ATLAS_OK, or ATLAS_EIO if the file exists but cannot be read.
 */
atlas_err_t atlas_profile_load(const char *dir, const char *id,
                               atlas_profile_t *p);

/**
 * Write `p` back to `dir`, creating the folder if needed.
 *
 * Goes through atlas_file_write_atomic(), so a failure partway leaves
 * the previous profile in place rather than a truncated one.
 *
 * @return ATLAS_OK, or the error from the write.
 */
atlas_err_t atlas_profile_save(const char *dir, const atlas_profile_t *p);

/**
 * Apply a profile's video fields on top of a configuration.
 *
 * Only the fields the profile actually sets are touched, which is what
 * makes a profile that names one key safe. The result is what should be
 * handed to atlas_video_apply() before launching.
 */
void atlas_profile_apply_video(const atlas_profile_t *p,
                               atlas_video_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_PROFILE_H */
