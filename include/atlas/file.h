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
