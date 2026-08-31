/*
 * AtlasPS2 - app.c
 * Discovering homebrew on the attached devices.
 */
#include <string.h>
#include <stdio.h>

/*
 * fileXio_rpc.h refuses to compile without this: the newlib port wants
 * you to use POSIX calls instead. We need fileXio because the dirent it
 * returns carries the mode, so one pass tells a folder from a file -
 * with opendir/stat that is a second round trip per entry, and a
 * Memory Card charges milliseconds for each one.
 */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <iox_stat.h>

#include "atlas/app.h"
#include "atlas/ini.h"
#include "atlas/path.h"
#include "atlas/log.h"

/* ------------------------------------------------------------------ */
/* Where to look                                                       */
/*                                                                     */
/* One root per device. Memory Cards keep applications inside the      */
/* ATLAS folder so the card stays tidy and an uninstall knows what is  */
/* ours; USB also gets a top-level APPS/, because that is where every  */
/* other PS2 launcher already looks and a user's existing stick should */
/* just work.                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    atlas_device_id_t device;
    const char       *rel;
} app_root_t;

static const app_root_t s_roots[] = {
    { ATLAS_DEV_MC0,  "ATLAS/APPS" },
    { ATLAS_DEV_MC1,  "ATLAS/APPS" },
    { ATLAS_DEV_MASS, "APPS"       },
    { ATLAS_DEV_MASS, "ATLAS/APPS" }
};

/*
 * How deep a scan goes. A root holds either loose ELFs or one folder
 * per application - that is the whole convention, so one level down is
 * enough. A deeper walk would let a user who dropped a backup of their
 * PC onto a stick stall the scan for minutes.
 */
#define APP_SCAN_DEPTH 1

/* Metadata files are a few hundred bytes; anything larger is not one
 * and is read no further rather than being allowed to grow the read. */
#define APP_INI_MAX 1024

/* ------------------------------------------------------------------ */
/* Catalogue                                                           */
/* ------------------------------------------------------------------ */

static atlas_app_t s_apps[ATLAS_APP_MAX];
static int         s_count;
static int         s_scanned;

/* ------------------------------------------------------------------ */
/* app.ini                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[ATLAS_APP_NAME_MAX];
    char elf[ATLAS_APP_PATH_MAX];
    char category[ATLAS_APP_CAT_MAX];
} app_meta_t;

static int meta_cb(void *user, const char *section,
                   const char *key, const char *value)
{
    app_meta_t *m = (app_meta_t *)user;

    /*
     * Keys outside [app] are ignored rather than rejected: a file may
     * carry sections for other tools, and refusing to read it because
     * another launcher also wrote to it would be hostile. Keys before
     * any section are accepted too, since a one-application file often
     * omits the header.
     */
    if (section[0] != '\0' && strcmp(section, "app") != 0)
        return 0;

    if (strcmp(key, "name") == 0)
        snprintf(m->name, sizeof(m->name), "%s", value);
    else if (strcmp(key, "elf") == 0)
        snprintf(m->elf, sizeof(m->elf), "%s", value);
    else if (strcmp(key, "category") == 0)
        snprintf(m->category, sizeof(m->category), "%s", value);

    return 0;
}

/**
 * Read and parse an app.ini, if there is one.
 *
 * A missing or unreadable file is not an error: most homebrew ships
 * without metadata, and that is the normal path through here.
 *
 * @return 1 when the file gave a name or an ELF, 0 otherwise.
 */
static int read_meta(const char *dir, app_meta_t *m)
{
    char path[ATLAS_APP_PATH_MAX];
    char buf[APP_INI_MAX];
    int fd, n;

    memset(m, 0, sizeof(*m));

    if (atlas_path_join(dir, "app.ini", path, sizeof(path)) != ATLAS_OK)
        return 0;

    fd = fileXioOpen(path, FIO_O_RDONLY);
    if (fd < 0)
        return 0;

    n = fileXioRead(fd, buf, (int)sizeof(buf));
    fileXioClose(fd);

    if (n <= 0)
        return 0;

    atlas_ini_parse(buf, n, meta_cb, m, NULL);

    return (m->name[0] != '\0' || m->elf[0] != '\0');
}

/* ------------------------------------------------------------------ */
/* Adding entries                                                      */
/* ------------------------------------------------------------------ */

static int has_elf_suffix(const char *name)
{
    int n = (int)strlen(name);
    const char *ext;

    if (n < 4)
        return 0;

    ext = name + n - 4;

    /* Case-insensitive: FAT volumes and Memory Cards disagree about it,
     * and "boot.elf" is as common as "BOOT.ELF". */
    return (ext[0] == '.')
        && (ext[1] == 'e' || ext[1] == 'E')
        && (ext[2] == 'l' || ext[2] == 'L')
        && (ext[3] == 'f' || ext[3] == 'F');
}

/**
 * Record one application.
 *
 * @param meta_name  name from app.ini, or NULL to derive one
 * @return 1 if stored, 0 if the catalogue is full or the path is too
 *         long to hold whole.
 */
static int add_app(const char *full_path, atlas_device_id_t device,
                   const char *meta_name, const char *category)
{
    atlas_app_t *a;

    if (s_count >= ATLAS_APP_MAX)
        return 0;

    /*
     * The path must survive whole or the entry is worse than useless:
     * launching a truncated path runs a different file, or nothing.
     */
    if ((int)strlen(full_path) >= ATLAS_APP_PATH_MAX)
        return 0;

    a = &s_apps[s_count];
    memset(a, 0, sizeof(*a));

    snprintf(a->path, sizeof(a->path), "%s", full_path);
    a->device = device;

    if (meta_name && meta_name[0]) {
        snprintf(a->name, sizeof(a->name), "%s", meta_name);
        a->has_metadata = 1;
    } else {
        atlas_path_pretty_name(full_path, a->name, sizeof(a->name));
        a->has_metadata = 0;
    }

    if (category && category[0])
        snprintf(a->category, sizeof(a->category), "%s", category);

    s_count++;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Scanning                                                            */
/* ------------------------------------------------------------------ */

static void scan_dir(const char *dir, atlas_device_id_t device, int depth,
                     const char *inherited_name, const char *inherited_cat);

/**
 * An application folder: an ELF plus optional metadata beside it.
 *
 * The ELF named by app.ini wins, so an author can ship several and
 * point at the right one. When that file is missing - a typo, or a
 * partial copy - the folder is scanned instead rather than the
 * application being dropped: a folder the user deliberately created
 * should appear even when its metadata is wrong.
 */
static void scan_app_folder(const char *dir, atlas_device_id_t device,
                            int depth)
{
    app_meta_t meta;
    char elf_path[ATLAS_APP_PATH_MAX];
    int fd;

    if (read_meta(dir, &meta) && meta.elf[0]) {
        if (atlas_path_join(dir, meta.elf, elf_path, sizeof(elf_path))
            == ATLAS_OK) {
            fd = fileXioOpen(elf_path, FIO_O_RDONLY);
            if (fd >= 0) {
                fileXioClose(fd);
                add_app(elf_path, device, meta.name, meta.category);
                return;
            }
            ATLAS_LOG("APP", "app.ini names a missing ELF: %s", elf_path);
        }
    }

    /* No usable `elf=`: fall back to whatever ELFs are in here. A name
     * the file did give still applies to them. */
    scan_dir(dir, device, depth, meta.name, meta.category);
}

static void scan_dir(const char *dir, atlas_device_id_t device, int depth,
                     const char *inherited_name, const char *inherited_cat)
{
    iox_dirent_t ent;
    int fd;

    fd = fileXioDopen(dir);
    if (fd < 0)
        return;

    while (fileXioDread(fd, &ent) > 0) {
        char child[ATLAS_APP_PATH_MAX];

        if (ent.name[0] == '\0' || strcmp(ent.name, ".") == 0
            || strcmp(ent.name, "..") == 0)
            continue;

        if (s_count >= ATLAS_APP_MAX)
            break;

        if (atlas_path_join(dir, ent.name, child, sizeof(child))
            != ATLAS_OK) {
            /* Too long to address at all. Skipping is the only safe
             * outcome: a shortened path names something else. */
            ATLAS_LOG("APP", "path too long, skipped: %s", ent.name);
            continue;
        }

        if (FIO_S_ISDIR(ent.stat.mode)) {
            if (depth > 0)
                scan_app_folder(child, device, depth - 1);
        } else if (has_elf_suffix(ent.name)) {
            add_app(child, device, inherited_name, inherited_cat);
        }
    }

    fileXioDclose(fd);
}

int atlas_app_scan(void)
{
    int previous = s_count;
    int i;

    s_count = 0;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_roots); i++) {
        const app_root_t *r = &s_roots[i];
        char root[ATLAS_APP_PATH_MAX];

        if (!atlas_device_is_ready(r->device))
            continue;

        if (atlas_device_path(r->device, r->rel, root, sizeof(root))
            != ATLAS_OK)
            continue;

        scan_dir(root, r->device, APP_SCAN_DEPTH, NULL, NULL);
    }

    /*
     * A rescan that finds nothing where the last one found something
     * usually means a device was pulled. The empty result still stands:
     * keeping the old list would leave rows the user launches into a
     * path that no longer resolves. Worth a line in a debug build.
     */
    if (s_count == 0 && previous > 0)
        ATLAS_LOG("APP", "rescan found nothing (was %d)", previous);

    s_scanned = 1;

    ATLAS_LOG("APP", "%d application(s)", s_count);
    return s_count;
}

int atlas_app_count(void)
{
    return s_count;
}

const atlas_app_t *atlas_app_get(int i)
{
    if (i < 0 || i >= s_count)
        return NULL;

    return &s_apps[i];
}

int atlas_app_scanned(void)
{
    return s_scanned;
}
