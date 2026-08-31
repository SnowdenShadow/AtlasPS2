/*
 * AtlasPS2 - path.h
 *
 * Path joining.
 *
 * Separate from the device layer so it can be checked on the host: this
 * is the function standing between the installer, the file manager and
 * the wrong file. Every rule here exists because getting it wrong
 * destroys user data rather than merely looking untidy.
 */
#ifndef ATLAS_PATH_H
#define ATLAS_PATH_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Join a device base and a relative path: ("mc0:", "ATLAS/ATLAS.INI")
 * gives "mc0:/ATLAS/ATLAS.INI".
 *
 * Leading slashes on `rel` are dropped, so a caller passing "/ATLAS"
 * does not produce "mc0://ATLAS" - some filesystems accept that and
 * some do not, and no caller should have to know which.
 *
 * Refuses rather than truncates. A silently shortened path names a
 * different file, one that may well exist, and the callers of this
 * delete and overwrite.
 *
 * `out` is left untouched on failure, so a caller that ignores the
 * return value cannot act on a half-built path.
 *
 * @return ATLAS_OK, or ATLAS_EINVAL for a bad argument or a result that
 *         does not fit in `size` (including its terminator).
 */
atlas_err_t atlas_path_join(const char *base, const char *rel,
                            char *out, int size);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_PATH_H */
