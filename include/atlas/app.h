/*
 * AtlasPS2 - app.h
 *
 * The application catalogue.
 *
 * Homebrew is discovered, never registered: the user copies a folder or
 * an ELF onto a device and it appears. Nothing in AtlasPS2 asks them to
 * edit a list, because a list is one more thing that goes stale when a
 * card is moved between consoles.
 *
 * The catalogue is a fixed array filled by a scan. It holds no pointers
 * into scanned data and allocates nothing, so a device pulled mid-scan
 * costs the entries from that device and nothing else.
 */
#ifndef ATLAS_APP_H
#define ATLAS_APP_H

#include "atlas/atlas.h"
#include "atlas/device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bounds. 128 entries is far more homebrew than a Memory Card holds and
 * costs ~48 KB of the EE's 32 MB - cheap enough to prefer over an
 * allocator that can fail halfway through a scan.
 */
#define ATLAS_APP_MAX        128
#define ATLAS_APP_NAME_MAX   48
#define ATLAS_APP_PATH_MAX   128
#define ATLAS_APP_CAT_MAX    24

typedef struct {
    /** Display name: from app.ini, else derived from the filename. */
    char name[ATLAS_APP_NAME_MAX];

    /** Full path to the ELF, ready for the launcher. */
    char path[ATLAS_APP_PATH_MAX];

    /** Category from app.ini, or "" when the file said nothing. */
    char category[ATLAS_APP_CAT_MAX];

    /** Which device it was found on, for the UI and for rescans. */
    atlas_device_id_t device;

    /** 1 when an app.ini supplied the name, 0 when it was derived. */
    int has_metadata;
} atlas_app_t;

/**
 * Rescan every ready device.
 *
 * Blocking, and slow enough to be visible: a directory walk over a
 * Memory Card takes a noticeable fraction of a second. Call it from an
 * explicit user action or once when a device appears - never per frame.
 *
 * The previous catalogue is replaced only once the scan finishes, so a
 * failure partway leaves the list that was already on screen intact
 * rather than emptying it.
 *
 * @return the number of applications found.
 */
int atlas_app_scan(void);

/** How many applications the last scan found. */
int atlas_app_count(void);

/** Entry `i`, or NULL if out of range. */
const atlas_app_t *atlas_app_get(int i);

/**
 * Whether a scan has ever run. The Applications screen distinguishes
 * "nothing here" from "not looked yet", which are different messages.
 */
int atlas_app_scanned(void);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_APP_H */
