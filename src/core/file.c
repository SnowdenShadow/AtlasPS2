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
#include "atlas/hash.h"
#include "atlas/log.h"

/* Long enough for "mc0:/ATLAS/CONFIG/ATLAS.INI.NEW" with room to spare;
 * a path that does not fit is refused rather than shortened. */
#define FILE_PATH_MAX 160

/*
 * The streaming buffer. 32 KB is static rather than on the stack - the
 * EE's default thread stack is not somewhere to put this - and is sized
 * for the Memory Card, which transfers in 16 KB pages through the IOP:
 * larger chunks stop paying for themselves, while smaller ones turn a
 * copy into a long sequence of RPCs.
 *
 * One buffer shared by copy and checksum. Neither reenters, and the
 * console has no second thread that could be in here at the same time.
 */
#define FILE_CHUNK 32768

static unsigned char s_chunk[FILE_CHUNK];

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

int atlas_file_size(const char *path)
{
    int fd, size;

    if (!path || !path[0])
        return -1;

    fd = fileXioOpen(path, FIO_O_RDONLY);
    if (fd < 0)
        return -1;

    /* Seek to the end rather than stat: fileXioGetstat is not
     * implemented by every driver behind these mount points, while
     * seeking works anywhere a file can be read at all. */
    size = fileXioLseek(fd, 0, FIO_SEEK_END);
    fileXioClose(fd);

    return size < 0 ? -1 : size;
}

atlas_err_t atlas_file_crc32(const char *path, u32 *out_crc,
                             atlas_file_progress_fn progress, void *ctx)
{
    u32 crc = ATLAS_CRC32_INIT;
    int fd, n, done = 0, total;

    if (!path || !path[0] || !out_crc)
        return ATLAS_EINVAL;

    total = atlas_file_size(path);

    fd = fileXioOpen(path, FIO_O_RDONLY);
    if (fd < 0)
        return ATLAS_ENOENT;

    for (;;) {
        n = fileXioRead(fd, s_chunk, FILE_CHUNK);

        if (n < 0) {
            fileXioClose(fd);
            return ATLAS_EIO;
        }

        if (n == 0)
            break;

        crc = atlas_crc32(crc, s_chunk, n);
        done += n;

        if (progress && !progress(done, total, ctx)) {
            fileXioClose(fd);
            return ATLAS_EBUSY;
        }
    }

    fileXioClose(fd);

    *out_crc = crc;
    return ATLAS_OK;
}

/**
 * Ask the driver behind `path` to commit what it is holding.
 *
 * Best effort: a device with no sync support returns an error and the
 * copy is no worse off than before, so nothing here is fatal.
 */
static void sync_device(const char *path)
{
    char dev[16];
    int i;

    for (i = 0; i < (int)sizeof(dev) - 2 && path[i] && path[i] != ':'; i++)
        dev[i] = path[i];

    if (path[i] != ':')
        return;              /* a relative path has no device to sync */

    dev[i] = ':';
    dev[i + 1] = '\0';

    fileXioSync(dev, FXIO_WAIT);
}

atlas_err_t atlas_file_copy(const char *src, const char *dst,
                            atlas_file_progress_fn progress, void *ctx)
{
    int in, out, n, w, done = 0, total;
    atlas_err_t err = ATLAS_OK;

    if (!src || !src[0] || !dst || !dst[0])
        return ATLAS_EINVAL;

    total = atlas_file_size(src);

    in = fileXioOpen(src, FIO_O_RDONLY);
    if (in < 0) {
        ATLAS_LOG("FILE", "cannot read %s (%d)", src, in);
        return ATLAS_ENOENT;
    }

    out = fileXioOpen(dst, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC, 0666);
    if (out < 0) {
        ATLAS_LOG("FILE", "cannot create %s (%d)", dst, out);
        fileXioClose(in);
        return ATLAS_EIO;
    }

    for (;;) {
        n = fileXioRead(in, s_chunk, FILE_CHUNK);

        if (n < 0) {
            err = ATLAS_EIO;
            break;
        }

        if (n == 0)
            break;

        w = fileXioWrite(out, s_chunk, n);

        /*
         * A short write is a full card, and it is reported as an error
         * rather than retried: retrying would spin on a device that has
         * nothing left to give, and the caller needs to hear "no space"
         * while it can still say so on screen.
         */
        if (w != n) {
            ATLAS_LOG("FILE", "short write on %s (%d of %d)", dst, w, n);
            err = w < 0 ? ATLAS_EIO : ATLAS_ENOSPC;
            break;
        }

        done += n;

        if (progress && !progress(done, total, ctx)) {
            err = ATLAS_EBUSY;
            break;
        }
    }

    fileXioClose(out);
    fileXioClose(in);

    /*
     * Flush the IOP's cached writes before anyone reads this back. A
     * verify that ran against a page still sitting in the driver would
     * prove nothing about what is on the card.
     *
     * fileXioSync wants the device, not the file - "mc0:", not
     * "mc0:/BOOT/BOOT.ELF" - so the prefix is cut out here.
     */
    if (err == ATLAS_OK)
        sync_device(dst);

    return err;
}

atlas_err_t atlas_file_copy_verified(const char *src, const char *dst,
                                     atlas_file_progress_fn progress,
                                     void *ctx)
{
    u32 crc_src, crc_dst;
    atlas_err_t err;

    err = atlas_file_copy(src, dst, progress, ctx);
    if (err != ATLAS_OK)
        return err;

    /*
     * The source is checksummed after the copy, not before: reading it
     * twice costs a pass, and doing it in this order means a source
     * that changed under us - a stick pulled mid-install - shows up as
     * a mismatch rather than as a silent success.
     */
    err = atlas_file_crc32(src, &crc_src, progress, ctx);
    if (err != ATLAS_OK)
        return err;

    err = atlas_file_crc32(dst, &crc_dst, progress, ctx);
    if (err != ATLAS_OK)
        return err;

    if (crc_src != crc_dst) {
        ATLAS_LOG("FILE", "verify failed on %s (%08x != %08x)",
                  dst, (unsigned)crc_src, (unsigned)crc_dst);

        /* Known-wrong is worse than absent: whatever looks here next
         * would find a file and assume it is good. */
        fileXioRemove(dst);
        return ATLAS_EFORMAT;
    }

    return ATLAS_OK;
}

atlas_err_t atlas_file_rename(const char *from, const char *to)
{
    if (!from || !from[0] || !to || !to[0])
        return ATLAS_EINVAL;

    if (fileXioRename(from, to) != 0) {
        ATLAS_LOG("FILE", "cannot rename %s to %s", from, to);
        return ATLAS_EIO;
    }

    return ATLAS_OK;
}

atlas_err_t atlas_file_remove(const char *path)
{
    if (!path || !path[0])
        return ATLAS_EINVAL;

    /* Not-there is the goal, so it is success. Callers use this to
     * clean up files that may never have been created. */
    if (!atlas_file_exists(path))
        return ATLAS_OK;

    if (fileXioRemove(path) != 0) {
        ATLAS_LOG("FILE", "cannot remove %s", path);
        return ATLAS_EIO;
    }

    return ATLAS_OK;
}
