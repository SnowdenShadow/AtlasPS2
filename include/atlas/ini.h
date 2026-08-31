/*
 * AtlasPS2 - ini.h
 *
 * A tolerant INI reader.
 *
 * Used for two things that must never fail hard: the per-application
 * `app.ini` metadata found on a user's Memory Card, and later the
 * environment's own configuration. Both are text files a user can edit
 * on a PC with any editor, so the parser assumes nothing about line
 * endings, spacing or key order, and a line it cannot make sense of is
 * skipped rather than aborting the file.
 *
 * It parses a buffer, not a path: the caller reads the file (these are
 * small) and owns the I/O, which keeps this module free of fileXio and
 * therefore checkable on the build machine.
 */
#ifndef ATLAS_INI_H
#define ATLAS_INI_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bounds. A key or value longer than these is not truncated - the whole
 * line is skipped and counted. Truncating a value silently would turn
 * "elf=VERYLONGNAME.ELF" into the name of a different file, and these
 * values end up as paths.
 */
#define ATLAS_INI_SECTION_MAX 32
#define ATLAS_INI_KEY_MAX     32
#define ATLAS_INI_VALUE_MAX   128

/**
 * Called once per accepted key.
 *
 * `section` is "" before the first [header]. Both `section` and `key`
 * are lowercased, because INI names are conventionally case-insensitive
 * and a user typing `Name=` should not be silently ignored. `value` is
 * verbatim apart from surrounding whitespace: it may be a filename, and
 * filenames are case-sensitive on FAT-backed paths.
 *
 * @return 0 to continue parsing, non-zero to stop early.
 */
typedef int (*atlas_ini_cb)(void *user, const char *section,
                            const char *key, const char *value);

/**
 * Parse `text` (need not be NUL-terminated; `len` bytes are read).
 *
 * Tolerated: CRLF or LF, blank lines, '#' and ';' comments, whitespace
 * around names and values, a missing ']' on a section header, and a
 * value containing '=' (only the first one splits).
 *
 * Skipped and counted in `bad_lines`: a line with no '=' outside a
 * section header, an empty key, and any line whose section, key or
 * value exceeds the bounds above.
 *
 * @param bad_lines  optional; receives the number of skipped lines, so a
 *                   caller can decide a file is too damaged to trust and
 *                   fall back to its .BAK copy.
 * @return ATLAS_OK, or ATLAS_EINVAL for a NULL argument or negative len.
 */
atlas_err_t atlas_ini_parse(const char *text, int len,
                            atlas_ini_cb cb, void *user, int *bad_lines);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_INI_H */
