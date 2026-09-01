/*
 * AtlasPS2 - fav_io.c
 * Reading and writing FAVORITES.INI.
 *
 * EE-only, and deliberately thin: the lists and every rule about them
 * are in fav.c, where `make check` can reach them.
 */
#include <string.h>
#include <stdio.h>

#include "atlas/fav.h"
#include "atlas/device.h"
#include "atlas/file.h"
#include "atlas/log.h"

/* Beside ATLAS.INI, and found the same way: Memory Cards before USB.
 * A card stays in the console, so that is where a list of the user's
 * own programs belongs. */
#define FAV_DIR  "ATLAS/CONFIG"
#define FAV_FILE "ATLAS/CONFIG/FAVORITES.INI"

static const atlas_device_id_t s_search[] = {
    ATLAS_DEV_MC0,
    ATLAS_DEV_MC1,
    ATLAS_DEV_MASS
};

#define FAV_PATH_MAX 160

void atlas_fav_clear_dirty_(void);   /* defined in fav.c */

void atlas_fav_load(void)
{
    /* Static: 6 KB is more than this function wants on the stack, and
     * it is called once at start-up. */
    static char buf[ATLAS_FAV_FILE_MAX];
    int i;

    atlas_fav_reset();

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search); i++) {
        char path[FAV_PATH_MAX];
        int len = 0;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (atlas_device_path(s_search[i], FAV_FILE, path, sizeof(path))
            != ATLAS_OK)
            continue;

        if (atlas_file_read(path, buf, (int)sizeof(buf), &len) != ATLAS_OK
            || len <= 0)
            continue;

        atlas_fav_parse(buf, len);

        ATLAS_LOG("FAV", "%s: %d favorite(s), %d recent",
                  path, atlas_fav_count(), atlas_recent_count());
        return;
    }

    ATLAS_LOG("FAV", "no favorites file found");
}

atlas_err_t atlas_fav_save(void)
{
    static char text[ATLAS_FAV_FILE_MAX];
    int len, i;

    /*
     * The whole point of the dirty flag. This is called from screen
     * transitions, and a user who opened the applications list and
     * came straight back must not have spent a card write on it.
     */
    if (!atlas_fav_dirty())
        return ATLAS_OK;

    len = atlas_fav_format(text, (int)sizeof(text));
    if (len < 0)
        return ATLAS_EFORMAT;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search); i++) {
        char dir[FAV_PATH_MAX];
        char path[FAV_PATH_MAX];
        atlas_err_t err;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (atlas_device_path(s_search[i], FAV_DIR, dir, sizeof(dir))
            != ATLAS_OK)
            continue;

        if (atlas_device_path(s_search[i], FAV_FILE, path, sizeof(path))
            != ATLAS_OK)
            continue;

        atlas_file_mkdir_p(dir);

        err = atlas_file_write_atomic(path, text, len);
        if (err == ATLAS_OK) {
            atlas_fav_clear_dirty_();
            ATLAS_LOG("FAV", "saved %s (%d bytes)", path, len);
            return ATLAS_OK;
        }

        ATLAS_LOG("FAV", "could not write %s (%d)", path, (int)err);
    }

    /*
     * Nothing could be written. The flag stays set on purpose: the
     * next save attempt - after the card is pushed back in, say -
     * still has something to write.
     */
    return ATLAS_ENODEV;
}
