/*
 * AtlasPS2 - fav.c
 * Favorites and the recently-used list: the lists themselves.
 *
 * The half that touches devices is in fav_io.c. This file holds no
 * fileXio and no gsKit, so `make check` on the build machine covers
 * every rule below - and the rules are the whole of it, since what a
 * favorites list does is entirely about which entry ends up where.
 */
#include <string.h>
#include <stdio.h>

#include "atlas/fav.h"
#include "atlas/ini.h"

/*
 * Fixed storage, deliberately. Both lists are small and bounded by the
 * header, and an allocation here would be an allocation made while the
 * user is pressing a button on a console with 32 MB and no swap.
 */
static char s_fav[ATLAS_FAV_MAX][ATLAS_APP_PATH_MAX];
static int  s_fav_n;

static char s_recent[ATLAS_RECENT_MAX][ATLAS_APP_PATH_MAX];
static int  s_recent_n;

/*
 * Set by every change, cleared by a load or a save. This is what stops
 * a user who walked through the applications screen without touching
 * anything from costing a Memory Card write.
 */
static int s_dirty;

void atlas_fav_reset(void)
{
    s_fav_n    = 0;
    s_recent_n = 0;
    s_dirty    = 0;
}

void atlas_fav_clear(void)
{
    /*
     * Same emptying, opposite intent. reset() is what a load starts
     * from and must not schedule a card write; clear() is a user
     * asking for the lists to be gone, and that has to reach the file
     * or it comes back on the next boot.
     */
    atlas_fav_reset();
    s_dirty = 1;
}

int atlas_fav_dirty(void)
{
    return s_dirty;
}

/* ------------------------------------------------------------------ */
/* List helpers                                                        */
/*                                                                     */
/* Paths are compared byte for byte. The device prefixes AtlasPS2      */
/* builds are its own ("mc0:/", "mass:/") and the rest comes from a    */
/* directory listing, so two spellings of one file do not arise from   */
/* anything this program does. Case-folding them would instead make    */
/* two genuinely different files on a case-sensitive host look like    */
/* one.                                                                */
/* ------------------------------------------------------------------ */

static int find_in(char (*list)[ATLAS_APP_PATH_MAX], int n, const char *path)
{
    int i;

    for (i = 0; i < n; i++) {
        if (strcmp(list[i], path) == 0)
            return i;
    }

    return -1;
}

/** Remove entry `i`, keeping the order of everything after it. */
static void remove_at(char (*list)[ATLAS_APP_PATH_MAX], int *n, int i)
{
    int j;

    for (j = i; j < *n - 1; j++)
        memcpy(list[j], list[j + 1], ATLAS_APP_PATH_MAX);

    (*n)--;
    list[*n][0] = '\0';
}

/**
 * Copy a path into a slot if it fits whole.
 *
 * A path that does not fit is refused rather than shortened: a
 * shortened path names a different file, or no file at all, and this
 * one is later handed to the loader.
 *
 * @return 1 if it was stored.
 */
static int store(char *slot, const char *path)
{
    if (!path || !path[0])
        return 0;

    if (strlen(path) >= ATLAS_APP_PATH_MAX)
        return 0;

    snprintf(slot, ATLAS_APP_PATH_MAX, "%s", path);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Favorites                                                           */
/* ------------------------------------------------------------------ */

int atlas_fav_is(const char *path)
{
    if (!path || !path[0])
        return 0;

    return find_in(s_fav, s_fav_n, path) >= 0;
}

int atlas_fav_toggle(const char *path)
{
    int i;

    if (!path || !path[0])
        return 0;

    i = find_in(s_fav, s_fav_n, path);

    if (i >= 0) {
        remove_at(s_fav, &s_fav_n, i);
        s_dirty = 1;
        return 0;
    }

    /*
     * A full list refuses quietly and reports "not a favorite", which
     * is what the user sees anyway: the star did not light. The
     * alternative - dropping someone else's favorite to make room -
     * would lose an entry the user never asked to lose.
     */
    if (s_fav_n >= ATLAS_FAV_MAX)
        return 0;

    if (!store(s_fav[s_fav_n], path))
        return 0;

    s_fav_n++;
    s_dirty = 1;
    return 1;
}

int atlas_fav_count(void)
{
    return s_fav_n;
}

const char *atlas_fav_get(int i)
{
    if (i < 0 || i >= s_fav_n)
        return NULL;

    return s_fav[i];
}

/* ------------------------------------------------------------------ */
/* Recently used                                                       */
/* ------------------------------------------------------------------ */

void atlas_recent_note(const char *path)
{
    int i, j;

    if (!path || !path[0] || strlen(path) >= ATLAS_APP_PATH_MAX)
        return;

    i = find_in(s_recent, s_recent_n, path);

    /*
     * Already the most recent thing launched. Re-writing the list here
     * would mark it dirty and spend a card write to store exactly what
     * is already stored - and relaunching the same program twice is
     * the single most common thing this list ever sees.
     */
    if (i == 0)
        return;

    if (i > 0)
        remove_at(s_recent, &s_recent_n, i);
    else if (s_recent_n >= ATLAS_RECENT_MAX)
        s_recent_n = ATLAS_RECENT_MAX - 1;   /* the oldest falls off */

    for (j = s_recent_n; j > 0; j--)
        memcpy(s_recent[j], s_recent[j - 1], ATLAS_APP_PATH_MAX);

    snprintf(s_recent[0], ATLAS_APP_PATH_MAX, "%s", path);
    s_recent_n++;
    s_dirty = 1;
}

int atlas_recent_count(void)
{
    return s_recent_n;
}

const char *atlas_recent_get(int i)
{
    if (i < 0 || i >= s_recent_n)
        return NULL;

    return s_recent[i];
}

/* ------------------------------------------------------------------ */
/* The file                                                            */
/*                                                                     */
/* One key repeated, rather than path1/path2/path3. The order in the   */
/* file is the order in the list, so a user who edits the file by hand */
/* reorders their favorites by moving lines - and nothing has to be    */
/* renumbered when they delete one.                                    */
/* ------------------------------------------------------------------ */

int atlas_fav_set(const char *section, const char *key, const char *value)
{
    if (!section || !key || !value)
        return 0;

    if (strcmp(key, "path") != 0)
        return 0;

    if (strcmp(section, "favorites") == 0) {
        if (s_fav_n >= ATLAS_FAV_MAX)
            return 1;   /* recognised, but there is no room for it */
        if (find_in(s_fav, s_fav_n, value) >= 0)
            return 1;   /* a duplicate in the file is not two stars */
        if (store(s_fav[s_fav_n], value))
            s_fav_n++;
        return 1;
    }

    if (strcmp(section, "recent") == 0) {
        if (s_recent_n >= ATLAS_RECENT_MAX)
            return 1;
        if (find_in(s_recent, s_recent_n, value) >= 0)
            return 1;
        if (store(s_recent[s_recent_n], value))
            s_recent_n++;
        return 1;
    }

    /* Unknown section ignored, as everywhere else: a file written by a
     * later version has to still load here. */
    return 0;
}

static int fav_cb(void *user, const char *section, const char *key,
                  const char *value)
{
    (void)user;
    atlas_fav_set(section, key, value);
    return 0;
}

atlas_err_t atlas_fav_parse(const char *text, int len)
{
    atlas_fav_reset();

    if (!text || len <= 0)
        return ATLAS_EINVAL;

    /*
     * Bad lines are not counted and not acted on. There is no backup
     * copy of this file to fall back to, and there does not need to be:
     * the worst a damaged favorites file can cost is a list the user
     * rebuilds by pressing Triangle a few times.
     */
    atlas_ini_parse(text, len, fav_cb, NULL, NULL);

    /* Loading is not a change. */
    s_dirty = 0;
    return ATLAS_OK;
}

int atlas_fav_format(char *out, int size)
{
    int n = 0, i, w;

    if (!out || size <= 0)
        return -1;

    w = snprintf(out, (size_t)size,
        "# " ATLAS_NAME " favorites and recently-used list\n"
        "#\n"
        "# Written by " ATLAS_NAME ", and safe to edit by hand: one\n"
        "# full path per line, in the order they appear on screen.\n"
        "# A path that no longer exists is kept and simply not shown,\n"
        "# so unplugging a USB stick does not empty this list.\n"
        "\n"
        "[favorites]\n");

    if (w < 0 || w >= size)
        return -1;
    n = w;

    for (i = 0; i < s_fav_n; i++) {
        w = snprintf(out + n, (size_t)(size - n), "path=%s\n", s_fav[i]);
        if (w < 0 || w >= size - n)
            return -1;
        n += w;
    }

    w = snprintf(out + n, (size_t)(size - n), "\n[recent]\n");
    if (w < 0 || w >= size - n)
        return -1;
    n += w;

    for (i = 0; i < s_recent_n; i++) {
        w = snprintf(out + n, (size_t)(size - n), "path=%s\n", s_recent[i]);
        if (w < 0 || w >= size - n)
            return -1;
        n += w;
    }

    return n;
}

/** Internal: fav_io.c clears this after a successful write. */
void atlas_fav_clear_dirty_(void);

void atlas_fav_clear_dirty_(void)
{
    s_dirty = 0;
}
