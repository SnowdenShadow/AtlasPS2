/*
 * AtlasPS2 - fs.h
 *
 * Listing a directory, and the rules that stand between the file
 * manager and a console that no longer boots.
 *
 * The whole-file operations live in file.h; this is the part that
 * browses. They are separate because file.h is used by the installer
 * and the configuration reader, neither of which has any business
 * walking a directory the user chose.
 *
 * WHAT IS DELIBERATELY MISSING
 * ----------------------------
 * There is no recursive delete. A file manager on a television, driven
 * by a D-pad, is the worst possible place to offer one: the target is
 * whatever row the cursor happened to be on, there is no undo, and the
 * directory the user is browsing may be their only copy of anything.
 * Removing a folder here means removing an empty folder; everything in
 * it has to be deleted where the user can see it.
 */
#ifndef ATLAS_FS_H
#define ATLAS_FS_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A directory with more entries than this is listed up to the limit and
 * the rest is reported. Truncating silently would show a user a folder
 * that looks complete and is not - and they might then delete "the last
 * copy" of something that was never on screen.
 */
#define ATLAS_FS_ENTRY_MAX 256

/* Long enough for any name a PS2 filesystem produces: Memory Cards cap
 * at 32, and FAT long names at 255 - which no PS2 driver returns whole
 * anyway. A name that does not fit is skipped, not shortened. */
#define ATLAS_FS_NAME_MAX 80

typedef struct {
    char name[ATLAS_FS_NAME_MAX];
    int  is_dir;
    int  size;      /**< bytes; meaningless for a directory */
} atlas_fs_entry_t;

/**
 * List `dir` into `out`, folders first and then files, each group in
 * case-insensitive alphabetical order.
 *
 * "." and ".." are not returned: the screen draws its own way up, and
 * a ".." that came from the driver would let the user walk above the
 * device root on some filesystems and not others.
 *
 * @param truncated optional; set to 1 if entries were left out.
 * @return the number of entries, or -1 if the directory could not be
 *         opened.
 */
int atlas_fs_list(const char *dir, atlas_fs_entry_t *out, int max,
                  int *truncated);

/**
 * Remove an empty directory.
 *
 * @return ATLAS_OK, ATLAS_ENOENT if it is not there, ATLAS_EBUSY if it
 *         still has something in it, ATLAS_EIO if the driver refused.
 */
atlas_err_t atlas_fs_rmdir(const char *path);

/**
 * Does this path name something the console needs in order to start?
 *
 * A yes does not forbid anything - it earns a second, differently
 * worded confirmation. The list is deliberately generous: the cost of
 * warning about a file that turned out to be replaceable is one extra
 * button press, and the cost of not warning about `mc0:/BOOT.ELF` is a
 * Memory Card that no longer boots and a user with no way to find out
 * why.
 *
 * Pure string handling, so `make check` covers it - which matters more
 * here than anywhere else in the program, because this is the function
 * that decides whether the user is warned at all.
 */
int atlas_fs_is_protected(const char *path);

/**
 * Is this path a device root ("mc0:/", "mass:/")?
 *
 * The row above the top of a device listing has to go back to the
 * device list rather than to a parent that does not exist.
 */
int atlas_fs_is_root(const char *path);

/**
 * Cut the last component off `path`, in place.
 *
 * Stops at the device root: "mc0:/ATLAS" becomes "mc0:/", and "mc0:/"
 * is left alone. A file manager that can walk off the top of a device
 * ends up asking the driver to list "mc0:" or "", and the answers to
 * those differ per driver.
 *
 * @return 1 if the path changed.
 */
int atlas_fs_parent(char *path);

/**
 * The last component of `path` - the filename, or the folder's own
 * name. Returns a pointer into `path`, never NULL for a valid string.
 */
const char *atlas_fs_basename(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_FS_H */
