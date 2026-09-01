/*
 * AtlasPS2 - fs.c
 * Listing a directory, and removing an empty one.
 *
 * EE-only. The path rules the file manager leans on are in fs_path.c,
 * where `make check` can reach them.
 */
#include <string.h>
#include <stdio.h>

/*
 * As in app.c: fileXio's dirent carries the mode, so one pass tells a
 * folder from a file. With opendir/stat that is a second round trip per
 * entry, and a Memory Card charges milliseconds for each one - on a
 * folder with two hundred files that is the difference between a
 * listing appearing and a listing being waited for.
 */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <iox_stat.h>

#include "atlas/fs.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Sorting                                                             */
/*                                                                     */
/* Folders before files, then case-insensitive alphabetical. A driver  */
/* returns entries in whatever order the filesystem stored them, which */
/* on a Memory Card is creation order - so an unsorted listing changes */
/* shape every time the user adds a file, and the row they aimed at is */
/* not the row they land on.                                           */
/* ------------------------------------------------------------------ */

static int name_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;

        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');

        if (ca != cb)
            return (ca < cb) ? -1 : 1;

        a++;
        b++;
    }

    if (*a == *b)
        return 0;

    return *a ? 1 : -1;
}

static int entry_cmp(const atlas_fs_entry_t *a, const atlas_fs_entry_t *b)
{
    if (a->is_dir != b->is_dir)
        return a->is_dir ? -1 : 1;

    return name_cmp(a->name, b->name);
}

/*
 * Insertion sort. At most ATLAS_FS_ENTRY_MAX entries, and the list is
 * usually far shorter; a quicksort here would be more code to get
 * wrong for a saving nobody can perceive.
 */
static void sort_entries(atlas_fs_entry_t *e, int n)
{
    int i, j;

    for (i = 1; i < n; i++) {
        atlas_fs_entry_t tmp = e[i];

        for (j = i; j > 0 && entry_cmp(&tmp, &e[j - 1]) < 0; j--)
            e[j] = e[j - 1];

        e[j] = tmp;
    }
}

/* ------------------------------------------------------------------ */
/* Listing                                                             */
/* ------------------------------------------------------------------ */

int atlas_fs_list(const char *dir, atlas_fs_entry_t *out, int max,
                  int *truncated)
{
    iox_dirent_t ent;
    int fd, n = 0;

    if (truncated)
        *truncated = 0;

    if (!dir || !dir[0] || !out || max <= 0)
        return -1;

    fd = fileXioDopen(dir);
    if (fd < 0)
        return -1;

    while (fileXioDread(fd, &ent) > 0) {
        if (ent.name[0] == '\0' || strcmp(ent.name, ".") == 0
            || strcmp(ent.name, "..") == 0)
            continue;

        if (n >= max) {
            if (truncated)
                *truncated = 1;
            break;
        }

        /*
         * A name too long to hold whole is skipped rather than cut. A
         * shortened name is a row the user can select and act on, and
         * the path built from it names a different file or none.
         */
        if (strlen(ent.name) >= ATLAS_FS_NAME_MAX)
            continue;

        memset(&out[n], 0, sizeof(out[n]));
        snprintf(out[n].name, sizeof(out[n].name), "%s", ent.name);
        out[n].is_dir = FIO_S_ISDIR(ent.stat.mode) ? 1 : 0;
        out[n].size   = out[n].is_dir ? 0 : (int)ent.stat.size;
        n++;
    }

    fileXioDclose(fd);

    sort_entries(out, n);

    return n;
}

/* ------------------------------------------------------------------ */
/* Removing a directory                                                */
/* ------------------------------------------------------------------ */

atlas_err_t atlas_fs_rmdir(const char *path)
{
    atlas_fs_entry_t probe;
    int fd, n;

    if (!path || !path[0])
        return ATLAS_EINVAL;

    fd = fileXioDopen(path);
    if (fd < 0)
        return ATLAS_ENOENT;
    fileXioDclose(fd);

    /*
     * Checked here rather than relying on the driver. Some Memory Card
     * implementations remove a non-empty directory and leave its
     * contents unreachable, which is the same as deleting them without
     * asking - exactly what this program promises never to do.
     */
    n = atlas_fs_list(path, &probe, 1, NULL);
    if (n > 0)
        return ATLAS_EBUSY;

    if (fileXioRmdir(path) < 0) {
        ATLAS_LOG("FS", "rmdir refused: %s", path);
        return ATLAS_EIO;
    }

    return ATLAS_OK;
}
