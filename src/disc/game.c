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
#include <libhdd.h>

#include "atlas/game.h"
#include "atlas/path.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Where to look on USB                                                */
/*                                                                     */
/* The internal HDD is scanned separately, below - it holds whole APA  */
/* partitions rather than files in folders, so it has no equivalent of */
/* this table.                                                         */
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

/* ------------------------------------------------------------------ */
/* HDL partitions on the internal HDD                                  */
/*                                                                     */
/* An HDLoader-style installer gives each game its own APA partition,  */
/* type 0x1337, separate from the __common partition device.c already  */
/* mounts. fileXioDopen("hdd0:") lists main partitions only - a sub's   */
/* own dread entry reports its main partition's name instead - so one   */
/* row per installed game falls out with no dedup of our own.           */
/*                                                                     */
/* Game data does not start at the partition's own first sector: HDL    */
/* reserves a 4 MB header (0x2000 512-byte sectors) ahead of it, a fact  */
/* confirmed by reading hdl-dump's inject_data(), not guessed. That      */
/* offset is added once, here, so hdl_start_lba is already the sector    */
/* where ISO data begins and nothing downstream repeats the arithmetic.  */
/* ------------------------------------------------------------------ */

static void scan_hdl(void)
{
    iox_dirent_t ent;
    int fd;

    fd = fileXioDopen("hdd0:");
    if (fd < 0)
        return;

    while (fileXioDread(fd, &ent) > 0) {
        atlas_game_t *g;
        char path[8 + sizeof(ent.name)];
        u32 sub = 0;
        int pfd, start, size;

        if (ent.name[0] == '\0' || ent.stat.mode != APA_TYPE_HDL)
            continue;

        if (s_count >= ATLAS_GAME_MAX)
            break;

        snprintf(path, sizeof(path), "hdd0:%s", ent.name);

        g = &s_games[s_count];
        memset(g, 0, sizeof(*g));
        snprintf(g->name, sizeof(g->name), "%.*s", (int)sizeof(g->name) - 1, ent.name);
        snprintf(g->path, sizeof(g->path), "%.*s", (int)sizeof(g->path) - 1, path);
        g->device = ATLAS_DEV_HDD;

        /*
         * private_0 is the subpartition count on a main partition's own
         * dirent. A multi-slice HDL install (very large or dual-layer
         * games split across several partitions) is a layout the boot
         * side's single contiguous run does not describe - listed so
         * the user sees it is there, but marked unstartable rather than
         * read wrong.
         */
        if (ent.stat.private_0 > 0) {
            g->is_hdl = 2;
            s_count++;
            continue;
        }

        pfd = fileXioOpen(path, FIO_O_RDONLY);
        if (pfd < 0) {
            g->is_hdl = 2;
            s_count++;
            continue;
        }

        start = fileXioIoctl2(pfd, HIOCGETPARTSTART, &sub, sizeof(sub),
                              NULL, 0);
        size  = fileXioIoctl2(pfd, HIOCGETSIZE, &sub, sizeof(sub),
                              NULL, 0);
        fileXioClose(pfd);

        /* A partition too small to hold HDL's own header is not a game
         * this installed, whatever its type byte claims. */
        if (start < 0 || size < 0 || (u32)size <= 0x2000) {
            g->is_hdl = 2;
            s_count++;
            continue;
        }

        g->is_hdl           = 1;
        g->hdl_start_lba     = (u32)start + 0x2000;
        g->hdl_total_sectors = (u32)size - 0x2000;
        s_count++;
    }

    fileXioDclose(fd);
}

int atlas_game_scan(void)
{
    unsigned int i;

    s_count = 0;

    /*
     * Not atlas_device_is_ready(ATLAS_DEV_HDD): that tracks whether
     * "__common" mounted, which a drive written purely by HDLoader-style
     * tools never has. HDL partitions are read by enumerating "hdd0:"
     * directly and need only the raw drive present and formatted.
     */
    if (atlas_device_hdd_present())
        scan_hdl();

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
