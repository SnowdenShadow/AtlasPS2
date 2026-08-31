/*
 * AtlasPS2 - file.h
 *
 * Whole-file read and replace, for the small text files AtlasPS2 owns:
 * ATLAS.INI, the language files, app.ini, and later the theme.
 *
 * WHY REPLACING IS ITS OWN OPERATION
 * ----------------------------------
 * Opening the real file for writing destroys it before the new content
 * exists. On a Memory Card the user can pull at any moment, and on a
 * console with no shutdown sequence, "the window between truncate and
 * write" is not theoretical - it is a card holding a zero-byte
 * ATLAS.INI and a launcher that boots to defaults with the user's
 * settings gone.
 *
 * So a write goes to a temporary name, the previous file becomes the
 * .BAK, and only then does the temporary take the real name. Every
 * moment in that sequence leaves at least one complete file on the
 * card. The same shape is what the update system uses for BOOT.ELF.
 */
#ifndef ATLAS_FILE_H
#define ATLAS_FILE_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Progress callback for the operations that take long enough to watch.
 *
 * Copying a 700 KB ELF to a Memory Card takes seconds. A user staring
 * at a frozen screen has no way to tell a slow write from a hung
 * console, and the reflex - pulling the card or cutting power - is the
 * one thing that turns a slow copy into a broken installation.
 *
 * @param done  bytes handled so far
 * @param total bytes expected, or -1 when the size is not known ahead
 * @param ctx   the caller's pointer, passed through untouched
 * @return non-zero to continue, 0 to abort the operation
 */
typedef int (*atlas_file_progress_fn)(int done, int total, void *ctx);

/**
 * Read a whole file into `buf`.
 *
 * For configuration-sized files only: a file larger than `size` is read
 * up to `size` and reported as ATLAS_EFORMAT rather than being parsed
 * in part, since a truncated INI can end mid-value.
 *
 * @param out_len  optional; receives the number of bytes read.
 * @return ATLAS_OK, ATLAS_ENOENT if it could not be opened, ATLAS_EIO
 *         on a read error, ATLAS_EFORMAT if it does not fit.
 */
atlas_err_t atlas_file_read(const char *path, void *buf, int size,
                            int *out_len);

/** Does the path exist and open for reading? */
int atlas_file_exists(const char *path);

/**
 * Replace `path` with `len` bytes of `data`, keeping the previous
 * contents as `<path>.BAK`.
 *
 * The sequence is: write `<path>.NEW`, delete the old `.BAK`, rename
 * `<path>` to `<path>.BAK`, rename `<path>.NEW` to `<path>`. A power
 * cut leaves either the old file, or the old file plus a complete
 * `.NEW`, or the new file plus a complete `.BAK` - never nothing.
 *
 * Parent directories are NOT created; the caller does that, because
 * only the caller knows which device it meant.
 *
 * @return ATLAS_OK, ATLAS_EINVAL for a bad argument or a path too long
 *         to append a suffix to, ATLAS_EIO if the write failed.
 */
atlas_err_t atlas_file_write_atomic(const char *path, const void *data,
                                    int len);

/** Size of `path` in bytes, or -1 if it cannot be opened. */
int atlas_file_size(const char *path);

/**
 * CRC-32 of a whole file, computed in chunks.
 *
 * The file is never held in RAM: a 700 KB ELF read into a buffer is 2%
 * of the console's memory for no reason, and the same routine has to
 * work on files this launcher does not control the size of.
 *
 * @param out_crc  receives the checksum; untouched on failure.
 * @return ATLAS_OK, ATLAS_ENOENT if it cannot be opened, ATLAS_EIO on a
 *         read error, ATLAS_EBUSY if `progress` asked to stop.
 */
atlas_err_t atlas_file_crc32(const char *path, u32 *out_crc,
                             atlas_file_progress_fn progress, void *ctx);

/**
 * Copy `src` to `dst`, overwriting it.
 *
 * Streams through a fixed buffer, so file size does not bound what can
 * be copied. The destination is left in place on failure rather than
 * removed: the caller is the only one that knows whether a partial file
 * at that path is dangerous, and for the installer it always is - which
 * is why it copies to a temporary name and verifies before renaming.
 *
 * Parent directories are NOT created; the caller does that.
 *
 * @return ATLAS_OK, ATLAS_ENOENT if `src` cannot be read, ATLAS_EIO if
 *         the write failed or came up short, ATLAS_EBUSY if `progress`
 *         asked to stop, ATLAS_EINVAL for a bad argument.
 */
atlas_err_t atlas_file_copy(const char *src, const char *dst,
                            atlas_file_progress_fn progress, void *ctx);

/**
 * Copy `src` to `dst` and prove the copy arrived intact.
 *
 * The destination is read back and its checksum compared against the
 * source's. A Memory Card with a failing sector accepts a write, reports
 * the right length, and returns different bytes afterwards; when the
 * file being copied is the one the console boots, that is a black
 * screen. Verifying costs a second pass over the file and removes the
 * whole class of failure.
 *
 * A destination that fails verification is deleted: a file that is known
 * wrong is more dangerous left in place than absent, because the next
 * thing to look at that path will find something and assume it is good.
 *
 * @return ATLAS_OK, ATLAS_EFORMAT if the checksums differ, otherwise
 *         the codes of atlas_file_copy().
 */
atlas_err_t atlas_file_copy_verified(const char *src, const char *dst,
                                     atlas_file_progress_fn progress,
                                     void *ctx);

/** Delete a file. Missing is success: the goal is that it is not there. */
atlas_err_t atlas_file_remove(const char *path);

/**
 * Rename `from` to `to` on the same device.
 *
 * This is how a staged file becomes the live one. Some Memory Card
 * drivers refuse to rename onto a name that already exists, so the
 * caller removes the destination first when it means to replace it -
 * this function does not, because for the installer's rollback the
 * destination being absent is precisely the invariant being relied on.
 *
 * @return ATLAS_OK, ATLAS_EINVAL for a bad argument, ATLAS_EIO if the
 *         device refused. Cross-device renames are not supported by the
 *         drivers underneath; use atlas_file_copy() for that.
 */
atlas_err_t atlas_file_rename(const char *from, const char *to);

/**
 * Create a directory and every missing parent above it.
 *
 * An existing directory is success, not an error: callers use this to
 * make sure a path is there, and racing against a folder the user
 * already created is the normal case.
 */
atlas_err_t atlas_file_mkdir_p(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_FILE_H */
