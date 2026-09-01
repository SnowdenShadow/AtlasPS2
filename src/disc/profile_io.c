/*
 * AtlasPS2 - profile_io.c
 * Reading and writing one title's profile.
 *
 * The half of profile.c that touches a device. Kept separate so the
 * parsing, the formatting and the clamps stay checkable on the build
 * machine, where getting them wrong is cheap to find.
 */
#include <stdio.h>
#include <string.h>

#include "atlas/profile.h"
#include "atlas/file.h"
#include "atlas/log.h"

atlas_err_t atlas_profile_load(const char *dir, const char *id,
                               atlas_profile_t *p)
{
    static char buf[ATLAS_PROFILE_FILE_MAX + 1];
    char path[192];
    atlas_err_t err;
    int len = 0;

    if (!dir || !id || !p)
        return ATLAS_EINVAL;

    atlas_profile_defaults(p);

    /*
     * The ID is stored before the read, not after it. A missing file
     * returns an empty profile, and an empty profile that still knows
     * which game it belongs to is what the caller needs in order to
     * offer to create one.
     */
    snprintf(p->id, sizeof(p->id), "%s", id);

    if (atlas_profile_path(dir, id, path, sizeof(path)) != ATLAS_OK)
        return ATLAS_EINVAL;

    err = atlas_file_read(path, buf, ATLAS_PROFILE_FILE_MAX, &len);

    /*
     * No file is the normal case: most games never need a profile, and
     * reporting that as an error would put a warning on screen for
     * every title in someone's collection.
     */
    if (err == ATLAS_ENOENT)
        return ATLAS_OK;

    /*
     * A file over the cap is refused rather than parsed to the cap.
     * Unlike the compatibility list - where a truncated read can only
     * lose whole entries from the end - a profile is a handful of keys,
     * so a file this size is not a long profile but a wrong file, and
     * applying the start of one is worse than applying none.
     */
    if (err == ATLAS_EFORMAT) {
        ATLAS_LOG("PROFILE", "%s is larger than %d bytes; ignored",
                  path, ATLAS_PROFILE_FILE_MAX);
        return ATLAS_OK;
    }

    if (err != ATLAS_OK)
        return ATLAS_EIO;

    buf[len] = '\0';

    return atlas_profile_parse(p, buf, len);
}

atlas_err_t atlas_profile_save(const char *dir, const atlas_profile_t *p)
{
    static char buf[ATLAS_PROFILE_FILE_MAX];
    char path[192];
    atlas_err_t err;
    int len;

    if (!dir || !p)
        return ATLAS_EINVAL;

    if (atlas_profile_path(dir, p->id, path, sizeof(path)) != ATLAS_OK)
        return ATLAS_EINVAL;

    len = atlas_profile_format(p, buf, sizeof(buf));

    if (len < 0)
        return ATLAS_ENOMEM;

    /* An existing folder is success here, per its contract. */
    err = atlas_file_mkdir_p(dir);

    if (err != ATLAS_OK)
        return err;

    /*
     * Atomic, like every other file AtlasPS2 writes: a card pulled
     * mid-write leaves the previous profile in place rather than a
     * half-written one that parses to something nobody chose.
     */
    return atlas_file_write_atomic(path, buf, len);
}
