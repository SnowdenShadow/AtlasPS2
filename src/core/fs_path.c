/*
 * AtlasPS2 - fs_path.c
 * The file manager's path rules: what is protected, and where "up" is.
 *
 * No fileXio here on purpose. This is the code that decides whether a
 * user gets warned before deleting the file their console boots from,
 * and it is checked by `make check` on the build machine rather than
 * being verified by wrecking a Memory Card.
 */
#include <string.h>

#include "atlas/fs.h"

/* ------------------------------------------------------------------ */
/* Where the device root is                                            */
/*                                                                     */
/* Every path this program builds starts with a device prefix and a    */
/* colon: "mc0:/...", "mass:/...". Finding the colon is therefore how  */
/* the root is found, and it is also what stops a walk upwards from    */
/* leaving the device.                                                 */
/* ------------------------------------------------------------------ */

/** Offset of the first character after "dev:", or -1 if there is none. */
static int root_offset(const char *path)
{
    const char *colon;

    if (!path || !path[0])
        return -1;

    colon = strchr(path, ':');
    if (!colon)
        return -1;

    /* The root includes the slash after the colon when one is there, so
     * "mc0:/" is the root and not "mc0:". Some drivers accept the
     * second spelling and some return nothing for it. */
    if (colon[1] == '/')
        return (int)(colon - path) + 2;

    return (int)(colon - path) + 1;
}

int atlas_fs_is_root(const char *path)
{
    int off = root_offset(path);

    if (off < 0)
        return 0;

    return path[off] == '\0';
}

int atlas_fs_parent(char *path)
{
    int off, i, last = -1;

    off = root_offset(path);
    if (off < 0)
        return 0;

    /* Already at the top. */
    if (path[off] == '\0')
        return 0;

    for (i = off; path[i]; i++) {
        if (path[i] == '/' && path[i + 1] != '\0')
            last = i;
    }

    /*
     * A trailing slash on the way up is dropped, so "mc0:/A/B/" and
     * "mc0:/A/B" both give "mc0:/A". Two spellings of one directory is
     * two rows in a listing that should have shown one.
     */
    if (last < 0) {
        path[off] = '\0';       /* one level down: back to the root */
        return 1;
    }

    path[last] = '\0';
    return 1;
}

const char *atlas_fs_basename(const char *path)
{
    const char *slash;

    if (!path)
        return "";

    slash = strrchr(path, '/');
    if (slash && slash[1] != '\0')
        return slash + 1;

    /* No slash after the device prefix: "mc0:/" or "mc0:FILE". */
    slash = strrchr(path, ':');
    if (slash)
        return slash + 1;

    return path;
}

/* ------------------------------------------------------------------ */
/* What must not be deleted without a second look                      */
/*                                                                     */
/* Two lists. The first is matched against the whole path from the     */
/* device root, because "BOOT.ELF" at the root of a card is the file   */
/* the console starts, and a BOOT.ELF three folders down is somebody's */
/* homebrew. The second is matched against any component, because      */
/* those are folders whose contents matter wherever they sit.          */
/* ------------------------------------------------------------------ */

static const char *const s_root_items[] = {
    "BOOT.ELF",         /* what the console runs at power-on */
    "BOOT.NEW",         /* a staged update, mid-transaction  */
    "BOOT.BAK",         /* the rollback copy                 */
    "BADISK.ELF",       /* the browser's own                 */
    "BAEXEC-SYSTEM",
    "BREXEC-SYSTEM",
    "BIEXEC-SYSTEM",
    "SYS-CONF",         /* the console's own settings folder */
    "BWNETCNF",
    "BADATA-SYSTEM",
    "BOOT",
    "APPS",
    "ATLAS"             /* our own installation              */
};

static const char *const s_anywhere[] = {
    "SYS-CONF",
    "BADATA-SYSTEM",
    "BWNETCNF"
};

/** Case-insensitive compare; FAT and Memory Cards disagree about case. */
static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;

        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');

        if (ca != cb)
            return 0;

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

/** Case-insensitive compare of `a` against the first `n` bytes of `b`. */
static int ieq_n(const char *a, const char *b, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];

        if (!ca)
            return 0;

        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');

        if (ca != cb)
            return 0;
    }

    return a[n] == '\0';
}

int atlas_fs_is_protected(const char *path)
{
    int off, i, start, len;
    const char *rest;

    off = root_offset(path);
    if (off < 0)
        return 0;

    rest = path + off;

    /*
     * The device root itself. Nothing offers to delete it, but the
     * copy and move targets do get checked, and "overwrite the root"
     * should never look like an ordinary destination.
     */
    if (rest[0] == '\0')
        return 1;

    /* First component from the root, compared whole. */
    for (len = 0; rest[len] && rest[len] != '/'; len++)
        ;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_root_items); i++) {
        if (ieq_n(s_root_items[i], rest, len))
            return 1;
    }

    /* Any component, anywhere down the tree. */
    start = 0;
    for (i = 0; ; i++) {
        if (rest[i] == '/' || rest[i] == '\0') {
            char comp[ATLAS_FS_NAME_MAX];
            int n = i - start;
            int j;

            if (n > 0 && n < (int)sizeof(comp)) {
                memcpy(comp, rest + start, (size_t)n);
                comp[n] = '\0';

                for (j = 0; j < ATLAS_ARRAY_COUNT(s_anywhere); j++) {
                    if (ieq(comp, s_anywhere[j]))
                        return 1;
                }
            }

            if (rest[i] == '\0')
                break;

            start = i + 1;
        }
    }

    return 0;
}
