/*
 * AtlasPS2 - log.h
 * Debug logging.
 *
 * Release builds compile the log calls out entirely (the strings do not
 * even reach the ELF), so debug instrumentation costs nothing at runtime
 * on the 32 MB the PS2 gives us. Debug builds print over the standard
 * ps2sdk stdout, which reaches ps2client / ps2link and PCSX2's console.
 */
#ifndef ATLAS_LOG_H
#define ATLAS_LOG_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ATLAS_DEBUG

/** Print one debug line. tag is a short subsystem name, e.g. "BOOT". */
void atlas_log(const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define ATLAS_LOG(tag, ...) atlas_log((tag), __VA_ARGS__)

#else /* !ATLAS_DEBUG */

/* The (void)0 keeps `if (x) ATLAS_LOG(...); else ...` valid. */
#define ATLAS_LOG(tag, ...) ((void)0)

#endif /* ATLAS_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_LOG_H */
