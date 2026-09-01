/*
 * AtlasPS2 - game.c
 * Finding disc images on the attached devices.
 */
#include <string.h>
#include <stdio.h>

/*
 * Same reason as app.c: fileXio's dirent carries the mode, so one pass
 * tells a folder from a file. With opendir/stat that is a second round
 * trip per entry, and a folder of thirty images pays it thirty times.
 */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <iox_stat.h>

#include "atlas/game.h"
#include "atlas/path.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Where to look                                                       */
/*                                                                     */
/* USB only, and only because that is the whole of what the drive      */
/* emulation reads today - see the scope note in docs/DISC.md. When    */
/* bd_read() grows HDD support this table is where it becomes visible  */
/* to the user, and until then listing an image somewhere unbootable   */
/* would be offering a row that always refuses.                        */
/*                                                                     */
/* The four folder names are the ones other PS2 launchers already use. */
/* A user arriving with an existing stick should not have to move      */
/* anything, and the root is included because plenty of people keep    */
/* images loose.                                                       */
/* ------------------------------------------------------------------ */

static const char *const s_roots[] = {
    "",         /* mass:/       */
    "DVD",      /* mass:/DVD/   */
    "CD",       /* mass:/CD/    */
    "ISO",      /* mass:/ISO/   */
    "ATLAS/GAMES"
};

/* ------------------------------------------------------------------ */
/* Catalogue                                                           */
/* ------------------------------------------------------------------ */

static atlas_game_t s_games[ATLAS_GAME_MAX];
static int          s_count;
static int          s_scanned;

/**
 * Does this filename end in an image extension we can open?
 *
 * Extension only. Opening every candidate to check its volume
 * descriptor would be the stall this scan exists to avoid, and a file
 * that is named .ISO and is not one fails in prepare(), on screen,
 * where the user can be told which file it was.
 */
static int has_image_suffix(const char *name)
{
    static const char *const ext[] = { ".iso", ".zso" };
    int n = (int)strlen(name);
    unsigned int e;

    if (n < 4)
        return 0;

    for (e = 0; e < sizeof(ext) / sizeof(ext[0]); e++) {
        const char *tail = name + n - 4;
        const char *want = ext[e];
        int i, match = 1;

        /* Case-insensitive: FAT volumes disagree about it, and both
         * GAME.ISO and game.iso are ordinary spellings. */
        for (i = 0; i < 4; i++) {
            char c = tail[i];

            if (c >= 'A' && c <= 'Z')
                c = (char)(c - 'A' + 'a');

            if (c != want[i]) {
                match = 0;
                break;
            }
        }

        if (match)
            return 1;
    }

    return 0;
}

static int already_listed(const char *path)
{
    int i;

    for (i = 0; i < s_count; i++)
        if (strcmp(s_games[i].path, path) == 0)
            return 1;

    return 0;
}

/**
 * Record one image.
 *
 * @return 1 if stored, 0 if the catalogue is full or the path is too
 *         long to hold whole - a truncated path names another file.
 */
static int add_game(const char *full_path, atlas_device_id_t device)
{
    atlas_game_t *g;

    if (s_count >= ATLAS_GAME_MAX)
        return 0;

    if ((int)strlen(full_path) >= ATLAS_GAME_PATH_MAX)
        return 0;

    /*
     * The roots overlap: "" is the volume root, and a user who keeps
     * images loose and also has a DVD/ folder gets both scanned. This
     * is where the same file being reached twice stops being two rows.
     */
    if (already_listed(full_path))
        return 0;

    g = &s_games[s_count];
    memset(g, 0, sizeof(*g));

    snprintf(g->path, sizeof(g->path), "%s", full_path);
    g->device = device;
    atlas_path_pretty_name(full_path, g->name, sizeof(g->name));

    s_count++;
    return 1;
}

/**
 * One directory, one level deep.
 *
 * No recursion. A stick's root can hold a backup of somebody's whole
 * PC, and a walk that descends into it turns entering this screen into
 * a minute of nothing happening.
 */
static void scan_dir(const char *dir, atlas_device_id_t device)
{
    iox_dirent_t ent;
    int fd;

    fd = fileXioDopen(dir);
    if (fd < 0)
        return;

    while (fileXioDread(fd, &ent) > 0) {
        char child[ATLAS_GAME_PATH_MAX];

        if (ent.name[0] == '\0' || FIO_S_ISDIR(ent.stat.mode))
            continue;

        if (!has_image_suffix(ent.name))
            continue;

        if (s_count >= ATLAS_GAME_MAX)
            break;

        if (atlas_path_join(dir, ent.name, child, sizeof(child))
            != ATLAS_OK) {
            ATLAS_LOG("GAME", "path too long, skipped: %s", ent.name);
            continue;
        }

        add_game(child, device);
    }

    fileXioDclose(fd);
}

int atlas_game_scan(void)
{
    unsigned int i;

    s_count = 0;

    if (atlas_device_is_ready(ATLAS_DEV_MASS)) {
        for (i = 0; i < sizeof(s_roots) / sizeof(s_roots[0]); i++) {
            char root[ATLAS_GAME_PATH_MAX];

            /*
             * The volume root is spelt "mass:/", with the separator.
             * atlas_device_path() joins onto "mass:" and adds no
             * separator for an empty relative part, which is right for
             * building a path and wrong for opening the root itself -
             * so that one case is spelt out here rather than by
             * loosening a helper the installer also uses.
             */
            if (s_roots[i][0] == '\0') {
                if (atlas_device_path(ATLAS_DEV_MASS, "", root,
                                      sizeof(root) - 1) != ATLAS_OK)
                    continue;

                {
                    int n = (int)strlen(root);

                    if (n == 0 || root[n - 1] != '/') {
                        root[n]     = '/';
                        root[n + 1] = '\0';
                    }
                }
            } else if (atlas_device_path(ATLAS_DEV_MASS, s_roots[i], root,
                                         sizeof(root)) != ATLAS_OK) {
                continue;
            }

            scan_dir(root, ATLAS_DEV_MASS);
        }
    }

    s_scanned = 1;

    ATLAS_LOG("GAME", "%d image(s)", s_count);
    return s_count;
}

int atlas_game_count(void)
{
    return s_count;
}

const atlas_game_t *atlas_game_get(int i)
{
    if (i < 0 || i >= s_count)
        return NULL;

    return &s_games[i];
}

int atlas_game_scanned(void)
{
    return s_scanned;
}
