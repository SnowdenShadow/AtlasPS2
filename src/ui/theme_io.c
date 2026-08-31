/*
 * AtlasPS2 - theme_io.c
 * Finding theme folders on the attached devices and loading one.
 *
 * EE-only, the same split as config_io.c: everything that decides what
 * a colour is lives in theme.c and is checked on the build machine,
 * and everything here only concerns itself with which device a file
 * came off.
 */
#include <string.h>
#include <stdio.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <iox_stat.h>

#include "atlas/theme.h"
#include "atlas/device.h"
#include "atlas/file.h"
#include "atlas/log.h"

/* Memory Cards before USB, the order the configuration uses: settings
 * belong to the console, not to whichever stick is plugged in. */
static const atlas_device_id_t s_search[] = {
    ATLAS_DEV_MC0,
    ATLAS_DEV_MC1,
    ATLAS_DEV_MASS
};

#define THEME_DIR "ATLAS/THEMES"

/* "mass:/ATLAS/THEMES/" plus a folder name plus "/theme.ini", with
 * room to spare. A path that does not fit is skipped, never cut. */
#define THEME_PATH_MAX 192

/** Declared in theme.c; records which theme is live. */
void atlas_theme_set_name_(const char *name);

atlas_err_t atlas_theme_load(const char *name)
{
    static char buf[ATLAS_THEME_FILE_MAX];
    int i;

    /* An empty name is how ATLAS.INI says "the built-in one". Not an
     * error, and not a search either. */
    if (!name || name[0] == '\0')
        return ATLAS_EINVAL;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search); i++) {
        char rel[THEME_PATH_MAX];
        char path[THEME_PATH_MAX];
        atlas_theme_t theme;
        int len = 0, applied = 0;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (snprintf(rel, sizeof(rel), "%s/%s/theme.ini", THEME_DIR, name)
            >= (int)sizeof(rel))
            continue;

        if (atlas_device_path(s_search[i], rel, path, sizeof(path))
            != ATLAS_OK)
            continue;

        if (atlas_file_read(path, buf, (int)sizeof(buf), &len) != ATLAS_OK
            || len <= 0)
            continue;

        if (atlas_theme_parse(&theme, buf, len, &applied) != ATLAS_OK)
            continue;

        /*
         * A file that set nothing is installed anyway. It parsed, so
         * every field holds the built-in value, and the result is the
         * default look under the user's chosen name - which is what
         * they will see and can then diagnose. Refusing here would
         * instead report "no such theme" for a theme that is plainly
         * sitting on the card.
         */
        atlas_theme_set(&theme);
        atlas_theme_set_name_(name);

        ATLAS_LOG("THEME", "loaded %s (%d/%d colours)", path, applied,
                  ATLAS_THEME_FIELD_COUNT);
        return ATLAS_OK;
    }

    ATLAS_LOG("THEME", "no device carries theme '%s'", name);
    return ATLAS_ENOENT;
}

/** Has this name already been listed from an earlier device? */
static int already_listed(char (*names)[ATLAS_THEME_NAME_MAX], int count,
                          const char *name)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return 1;
    }

    return 0;
}

int atlas_theme_list(char (*names)[ATLAS_THEME_NAME_MAX], int max)
{
    int count = 0;
    int i;

    if (!names || max <= 0)
        return 0;

    for (i = 0; i < ATLAS_ARRAY_COUNT(s_search) && count < max; i++) {
        char root[THEME_PATH_MAX];
        iox_dirent_t ent;
        int fd;

        if (!atlas_device_is_ready(s_search[i]))
            continue;

        if (atlas_device_path(s_search[i], THEME_DIR, root, sizeof(root))
            != ATLAS_OK)
            continue;

        fd = fileXioDopen(root);
        if (fd < 0)
            continue;  /* no THEMES folder here, which is normal */

        while (count < max && fileXioDread(fd, &ent) > 0) {
            char probe[THEME_PATH_MAX];

            if (ent.name[0] == '\0' || strcmp(ent.name, ".") == 0
                || strcmp(ent.name, "..") == 0)
                continue;

            if (!FIO_S_ISDIR(ent.stat.mode))
                continue;

            /* Too long to address, so too long to load: listing it
             * would offer the user something that cannot be selected. */
            if ((int)strlen(ent.name) >= ATLAS_THEME_NAME_MAX)
                continue;

            /* The same theme on a card and a stick is one entry. The
             * card's copy is what atlas_theme_load() would find, and
             * two identical rows is a menu the user cannot choose
             * between. */
            if (already_listed(names, count, ent.name))
                continue;

            /*
             * A folder is only a theme if it holds a theme.ini. Without
             * this check any stray directory under THEMES/ becomes a
             * menu entry that does nothing when selected.
             */
            if (snprintf(probe, sizeof(probe), "%s/%s/theme.ini", root,
                         ent.name) >= (int)sizeof(probe))
                continue;

            if (!atlas_file_exists(probe))
                continue;

            snprintf(names[count], ATLAS_THEME_NAME_MAX, "%s", ent.name);
            count++;
        }

        fileXioDclose(fd);
    }

    return count;
}
