/*
 * AtlasPS2 - tests/host/fileXio_rpc.h
 *
 * A stand-in for the PS2SDK header, backed by stdio, so image.c can be
 * checked on the build machine without being modified for it.
 *
 * WHY A STUB RATHER THAN A SEAM IN THE MODULE
 * -------------------------------------------
 * image.c could have taken its reads through a callback the way disc.c
 * does, and then this file would not exist. It does not, because the
 * thing worth checking is the ZSO index arithmetic - which block holds
 * sector N, where that block starts, how long it is - and a seam would
 * let the check run against a version of that arithmetic wired up
 * differently from the one the console runs. The bytes the console
 * reads come through fileXio, so the check reads them through something
 * shaped exactly like fileXio.
 *
 * Only the five calls image.c makes are here. This models a file, not
 * the IOP: no RPC, no async, no error codes beyond "it failed".
 */
#ifndef ATLAS_HOST_FILEXIO_RPC_H
#define ATLAS_HOST_FILEXIO_RPC_H

#include <stdio.h>

#define FIO_O_RDONLY 0x0001

/* Open descriptors. The count is small on purpose: a check that leaks
 * one should run out and say so, rather than hiding the leak. */
#define ATLAS_HOST_FD_MAX 8

static FILE *atlas_host_fd[ATLAS_HOST_FD_MAX];

static int fileXioOpen(const char *path, int flags)
{
    int i;

    (void)flags;

    for (i = 0; i < ATLAS_HOST_FD_MAX; i++) {
        if (!atlas_host_fd[i]) {
            atlas_host_fd[i] = fopen(path, "rb");
            return atlas_host_fd[i] ? i : -1;
        }
    }

    return -1;
}

static int fileXioClose(int fd)
{
    if (fd < 0 || fd >= ATLAS_HOST_FD_MAX || !atlas_host_fd[fd])
        return -1;

    fclose(atlas_host_fd[fd]);
    atlas_host_fd[fd] = NULL;
    return 0;
}

static int fileXioRead(int fd, void *buf, int len)
{
    if (fd < 0 || fd >= ATLAS_HOST_FD_MAX || !atlas_host_fd[fd])
        return -1;

    return (int)fread(buf, 1, (size_t)len, atlas_host_fd[fd]);
}

static int fileXioLseek(int fd, int offset, int whence)
{
    if (fd < 0 || fd >= ATLAS_HOST_FD_MAX || !atlas_host_fd[fd])
        return -1;

    if (fseek(atlas_host_fd[fd], offset, whence) != 0)
        return -1;

    return (int)ftell(atlas_host_fd[fd]);
}

#endif /* ATLAS_HOST_FILEXIO_RPC_H */
