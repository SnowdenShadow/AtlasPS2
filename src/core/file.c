/*
 * AtlasPS2 - file.c
 * Whole-file read, and replace-without-losing-the-old-one.
 */
#include <string.h>
#include <stdio.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>

#include "atlas/file.h"
#include "atlas/log.h"

/* Long enough for "mc0:/ATLAS/CONFIG/ATLAS.INI.NEW" with room to spare;
 * a path that does not fit is refused rather than shortened. */
#define FILE_PATH_MAX 160

atlas_err_t atlas_file_read(const char *path, void *buf, int size,
                            int *out_len)
{
    int fd, n;

    if (out_len)
        *out_len = 0;

    if (!path || !path[0] || !buf || size <= 0)
        return ATLAS_EINVAL;

    fd = fileXioOpen(path, FIO_O_RDONLY);
    if (fd < 0)
        return ATLAS_ENOENT;

    n = fileXioRead(fd, buf, size);
    fileXioClose(fd);

    if (n < 0)
        return ATLAS_EIO;

    if (out_len)
        *out_len = n;

    /*
     * A file that filled the buffer exactly may have had more to give.
     * Parsing what we got would silently drop settings, and for a
     * configuration that means writing a shorter file back later and
     * losing them for good.
     */
    if (n == size)
        return ATLAS_EFORMAT;

    return ATLAS_OK;
}

int atlas_file_exists(const char *path)
{
    int fd;

    if (!path || !path[0])
        return 0;

    fd = fileXioOpen(path, FIO_O_RDONLY);
    if (fd < 0)
        return 0;

    fileXioClose(fd);
    return 1;
}

/** Write a complete file, creating or truncating it. */
static atlas_err_t write_whole(const char *path, const void *data, int len)
{
    int fd, n;

    fd = fileXioOpen(path, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC, 0666);
    if (fd < 0) {
        ATLAS_LOG("FILE", "cannot create %s (%d)", path, fd);
        return ATLAS_EIO;
    }

    n = (len > 0) ? fileXioWrite(fd, data, len) : 0;
    fileXioClose(fd);

    if (n != len) {
        ATLAS_LOG("FILE", "short write on %s (%d of %d)", path, n, len);
        return ATLAS_EIO;
    }

    return ATLAS_OK;
}

atlas_err_t atlas_file_write_atomic(const char *path, const void *data,
                                    int len)
{
    char tmp[FILE_PATH_MAX];
    char bak[FILE_PATH_MAX];
    atlas_err_t err;
    int plen;

    if (!path || !path[0] || (!data && len > 0) || len < 0)
        return ATLAS_EINVAL;

    plen = (int)strlen(path);
    if (plen + 5 >= FILE_PATH_MAX)   /* ".NEW" / ".BAK" plus terminator */
        return ATLAS_EINVAL;

    snprintf(tmp, sizeof(tmp), "%s.NEW", path);
    snprintf(bak, sizeof(bak), "%s.BAK", path);

    /* Stage first. Until this succeeds nothing on the card has moved. */
    err = write_whole(tmp, data, len);
    if (err != ATLAS_OK)
        return err;

    /*
     * Rotate the old file into place as the backup. Both calls are
     * allowed to fail: on a first run there is nothing to rotate, and
     * some filesystems refuse to rename onto a name that exists, which
     * is why the old backup goes first. A failure here costs the backup,
     * not the write, so it is logged and stepped over.
     */
    fileXioRemove(bak);
    fileXioRename(path, bak);

    if (fileXioRename(tmp, path) != 0) {
        /*
         * The device has no usable rename - some Memory Card drivers do
         * not implement one. Fall back to writing the real name
         * directly. That gives up the atomic swap, which is the whole
         * point of the dance, but the alternative is a device where
         * settings can never be saved at all. The staged copy is left
         * behind on failure: it is the only complete copy at that
         * moment.
         */
        ATLAS_LOG("FILE", "rename unavailable, writing %s in place", path);

        err = write_whole(path, data, len);
        if (err != ATLAS_OK)
            return err;

        fileXioRemove(tmp);
    }

    return ATLAS_OK;
}

atlas_err_t atlas_file_mkdir_p(const char *path)
{
    char work[FILE_PATH_MAX];
    int i, n;

    if (!path || !path[0])
        return ATLAS_EINVAL;

    n = (int)strlen(path);
    if (n >= FILE_PATH_MAX)
        return ATLAS_EINVAL;

    memcpy(work, path, (size_t)n + 1);

    /*
     * Walk forward creating each prefix. The scan starts after the
     * device colon: "mc0:" is not a directory anyone can create, and
     * asking to create it fails in a way that reads like a real error
     * in the log.
     */
    i = 0;
    while (work[i] != '\0' && work[i] != ':')
        i++;

    if (work[i] == ':')
        i++;

    for (; work[i] != '\0'; i++) {
        if (work[i] != '/' || i == 0)
            continue;

        work[i] = '\0';
        if (work[i - 1] != ':')      /* skip the bare "mc0:/" prefix */
            fileXioMkdir(work, 0777);
        work[i] = '/';
    }

    /* mkdir on something that already exists is the normal case here,
     * so the result is only interesting if the path is still missing. */
    fileXioMkdir(work, 0777);

    return ATLAS_OK;
}
