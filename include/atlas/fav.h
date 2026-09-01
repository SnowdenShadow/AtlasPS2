/*
 * AtlasPS2 - fav.h
 *
 * Favorites and the recently-used list.
 *
 * Both are lists of ELF paths, kept in one small file beside ATLAS.INI.
 * A path is the identity: it survives a rescan, it survives an app.ini
 * being renamed, and it is what tells two copies of the same homebrew
 * on two devices apart. An index into the catalogue would not survive
 * anything - the catalogue is rebuilt whenever a device appears.
 *
 * WHY A PATH THAT NO LONGER EXISTS IS KEPT
 * ----------------------------------------
 * A USB stick that is not plugged in makes every path on it dangle.
 * Dropping those entries would mean unplugging a stick silently
 * emptied the user's favorites, and plugging it back in would not
 * bring them back. So entries are kept and the screens show only the
 * ones they can currently resolve.
 *
 * WHY WRITES ARE COUNTED
 * ----------------------
 * A Memory Card write is slow and finite. Nothing here writes on its
 * own: the lists are marked dirty and the caller flushes once, at a
 * point where a pause is expected anyway.
 */
#ifndef ATLAS_FAV_H
#define ATLAS_FAV_H

#include "atlas/atlas.h"
#include "atlas/app.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A favorites list longer than this is a list nobody scrolls through,
 * and the pair costs ~10 KB of the EE's 32 MB - cheap enough to prefer
 * over an allocator that can fail while the user is pressing a button.
 */
#define ATLAS_FAV_MAX    32
#define ATLAS_RECENT_MAX 8

/** Empty both lists. Does not touch any device. */
void atlas_fav_reset(void);

/* ------------------------------------------------------------------ */
/* Favorites                                                           */
/* ------------------------------------------------------------------ */

/** Is this path marked? */
int atlas_fav_is(const char *path);

/**
 * Add or remove a favorite.
 *
 * @return 1 if it is a favorite afterwards, 0 if it is not - including
 *         the case where the list was full, which is reported the same
 *         way a removal is because the outcome the user sees is the
 *         same: the star did not light.
 */
int atlas_fav_toggle(const char *path);

/** How many favorites are stored, resolvable or not. */
int atlas_fav_count(void);

/** Favorite `i`, or NULL. Ordered by when it was added. */
const char *atlas_fav_get(int i);

/* ------------------------------------------------------------------ */
/* Recently used                                                       */
/* ------------------------------------------------------------------ */

/**
 * Record that this path was launched: it moves to the front.
 *
 * Called before the handover, not after - after there is no `after`,
 * because the ELF has replaced us.
 */
void atlas_recent_note(const char *path);

/** How many entries the recent list holds. */
int atlas_recent_count(void);

/** Recent entry `i`, most recent first, or NULL. */
const char *atlas_recent_get(int i);

/* ------------------------------------------------------------------ */
/* Storage                                                             */
/* ------------------------------------------------------------------ */

/** Have the lists changed since the last load or save? */
int atlas_fav_dirty(void);

/**
 * Apply one `key = value` pair from a favorites file.
 *
 * Exposed so the self-checks can drive the whole mapping without a
 * filesystem.
 *
 * @return 1 if the key was recognised.
 */
int atlas_fav_set(const char *section, const char *key, const char *value);

/** Parse a whole favorites file over empty lists. */
atlas_err_t atlas_fav_parse(const char *text, int len);

/**
 * Render the lists as the text of a favorites file.
 *
 * @return bytes written, or -1 if `size` was too small.
 */
int atlas_fav_format(char *out, int size);

/** The longest favorites file this module will read or write. */
#define ATLAS_FAV_FILE_MAX 6144

/**
 * Read the lists from the first device that has them.
 *
 * Never fails in a way the caller has to handle: no file means no
 * favorites, which is what a fresh install has anyway.
 */
void atlas_fav_load(void);

/**
 * Write the lists back, if anything changed.
 *
 * A no-op when nothing is dirty, which is the point: this is called
 * from screen transitions and must not spend a Memory Card write on a
 * user who only looked.
 *
 * @return ATLAS_OK (including when there was nothing to do), or the
 *         error from the write.
 */
atlas_err_t atlas_fav_save(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_FAV_H */
