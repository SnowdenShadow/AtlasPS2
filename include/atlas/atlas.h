/*
 * AtlasPS2 - PlayStation 2 Home Environment
 * atlas.h - Core types, version and error codes.
 *
 * Licensed under the MIT License. See LICENSE.
 */
#ifndef ATLAS_H
#define ATLAS_H

#include <tamtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Version                                                             */
/* ------------------------------------------------------------------ */

#define ATLAS_VERSION_MAJOR 0
#define ATLAS_VERSION_MINOR 1
#define ATLAS_VERSION_PATCH 0

#define ATLAS_STR2(x) #x
#define ATLAS_STR(x)  ATLAS_STR2(x)

#define ATLAS_VERSION_STRING           \
    ATLAS_STR(ATLAS_VERSION_MAJOR) "." \
    ATLAS_STR(ATLAS_VERSION_MINOR) "." \
    ATLAS_STR(ATLAS_VERSION_PATCH)

#define ATLAS_NAME "AtlasPS2"

/* ------------------------------------------------------------------ */
/* Error codes                                                         */
/*                                                                     */
/* Every subsystem returns atlas_err_t. Negative == failure. The rule  */
/* across AtlasPS2 is: a failing optional subsystem degrades features, */
/* it never aborts the boot. Only ATLAS_EFATAL justifies stopping.     */
/* ------------------------------------------------------------------ */

typedef enum {
    ATLAS_OK        =  0,  /* success                                  */
    ATLAS_EFAIL     = -1,  /* generic failure                          */
    ATLAS_ENOENT    = -2,  /* not found                                */
    ATLAS_EIO       = -3,  /* device / filesystem I/O error            */
    ATLAS_ENOMEM    = -4,  /* out of memory                            */
    ATLAS_EINVAL    = -5,  /* invalid argument                         */
    ATLAS_ENODEV    = -6,  /* device absent or not mounted             */
    ATLAS_EBUSY     = -7,  /* resource busy                            */
    ATLAS_ETIMEOUT  = -8,  /* operation timed out                      */
    ATLAS_ENOSPC    = -9,  /* not enough free space                    */
    ATLAS_EPERM     = -10, /* refused (protected path, read-only, ...) */
    ATLAS_EFORMAT   = -11, /* corrupt or unparsable data               */
    ATLAS_EFATAL    = -12  /* unrecoverable; caller must bail out      */
} atlas_err_t;

/** Human readable name for an error code. Never returns NULL. */
const char *atlas_err_str(atlas_err_t err);

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

#define ATLAS_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

#define ATLAS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define ATLAS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ATLAS_CLAMP(v, lo, hi) ATLAS_MIN(ATLAS_MAX((v), (lo)), (hi))

/** Mark a parameter as deliberately unused (keeps -Wall quiet). */
#define ATLAS_UNUSED(x) ((void)(x))

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_H */
