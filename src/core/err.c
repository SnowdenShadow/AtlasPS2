/*
 * AtlasPS2 - err.c
 * Error code to string mapping.
 */
#include "atlas/atlas.h"

const char *atlas_err_str(atlas_err_t err)
{
    switch (err) {
    case ATLAS_OK:       return "OK";
    case ATLAS_EFAIL:    return "failure";
    case ATLAS_ENOENT:   return "not found";
    case ATLAS_EIO:      return "I/O error";
    case ATLAS_ENOMEM:   return "out of memory";
    case ATLAS_EINVAL:   return "invalid argument";
    case ATLAS_ENODEV:   return "device unavailable";
    case ATLAS_EBUSY:    return "busy";
    case ATLAS_ETIMEOUT: return "timed out";
    case ATLAS_ENOSPC:   return "not enough space";
    case ATLAS_EPERM:    return "not permitted";
    case ATLAS_EFORMAT:  return "corrupt data";
    case ATLAS_EFATAL:   return "fatal error";
    }
    return "unknown error";
}
