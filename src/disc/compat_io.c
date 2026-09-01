/*
 * AtlasPS2 - compat_io.c
 * Reading the compatibility list off a device.
 *
 * The other half of compat.c, split out for the same reason theme_io.c
 * is split from theme.c: everything that decides what a setting *means*
 * is checkable on the build machine, and only the part that touches
 * fileXio is not. What is left here is a read and a size cap.
 */
#include "atlas/compat.h"
#include "atlas/file.h"
#include "atlas/log.h"

/*
 * The file is INI, and INI values are small. This is sized for a list
 * a user maintains themselves - a few dozen games - rather than for a
 * shared database, which is what would hit the cap.
 */
#define COMPAT_FILE_MAX 8192

atlas_err_t atlas_compat_load(const char *path, int *out_count)
{
    static char buf[COMPAT_FILE_MAX + 1];
    atlas_err_t err;
    int len = 0;

    if (out_count)
        *out_count = 0;

    if (!path || !path[0])
        return ATLAS_EINVAL;

    atlas_compat_clear();

    err = atlas_file_read(path, buf, COMPAT_FILE_MAX, &len);

    /*
     * No file is not a problem. It means no game has needed a
     * workaround yet, which is true of a fresh installation and of most
     * users forever - and an error here would put a warning on screen
     * for the normal case.
     */
    if (err == ATLAS_ENOENT)
        return ATLAS_OK;

    /*
     * A file too large to hold is read up to the cap and parsed anyway.
     * The alternative is discarding every entry because the list grew,
     * and a truncated INI can only lose whole settings from the end -
     * unlike a config file, where a half-read value would be applied.
     */
    if (err == ATLAS_EFORMAT) {
        ATLAS_LOG("COMPAT", "%s is larger than %d bytes; using the start",
                  path, COMPAT_FILE_MAX);
    } else if (err != ATLAS_OK) {
        return ATLAS_EIO;
    }

    buf[len] = '\0';

    return atlas_compat_parse(buf, len, out_count);
}
