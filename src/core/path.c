/*
 * AtlasPS2 - path.c
 * Path joining.
 */
#include <string.h>

#include "atlas/path.h"

atlas_err_t atlas_path_join(const char *base, const char *rel,
                            char *out, int size)
{
    int base_len, rel_len, need, sep;

    if (!base || !rel || !out || size <= 0)
        return ATLAS_EINVAL;

    /*
     * Drop leading slashes so "/ATLAS" cannot produce "mc0://ATLAS":
     * some filesystems accept a doubled separator and some do not, and
     * no caller should have to know which.
     */
    while (*rel == '/')
        rel++;

    base_len = (int)strlen(base);
    rel_len  = (int)strlen(rel);

    /*
     * A trailing ':' does NOT count as a separator: "mc0:" is the
     * device, and the path on it starts at "mc0:/". That is the form
     * used throughout the project and in every path shown to the user.
     * A trailing '/' does count, so joining does not double it.
     *
     * An empty relative part needs no separator either: the result is
     * the device root itself.
     */
    sep = 1;
    if (rel_len == 0)
        sep = 0;
    else if (base_len > 0 && base[base_len - 1] == '/')
        sep = 0;

    need = base_len + sep + rel_len + 1; /* + NUL */

    /*
     * Refuse rather than truncate, and leave `out` alone while doing
     * so: a caller that ignores the return value then cannot act on a
     * partially written buffer.
     */
    if (need > size)
        return ATLAS_EINVAL;

    memcpy(out, base, (size_t)base_len);

    if (sep)
        out[base_len] = '/';

    memcpy(out + base_len + sep, rel, (size_t)rel_len);
    out[base_len + sep + rel_len] = '\0';

    return ATLAS_OK;
}

atlas_err_t atlas_path_pretty_name(const char *filename, char *out, int size)
{
    const char *base, *dot, *p;
    int n, i;

    if (!filename || !out || size <= 0)
        return ATLAS_EINVAL;

    /* Both separators: a path may come from a FAT volume. */
    base = filename;
    for (p = filename; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':')
            base = p + 1;
    }

    /*
     * A trailing separator leaves nothing after it. Fall back to the
     * whole input: a directory path is not what this is for, but an
     * empty label is a row the user cannot identify, and that is the
     * one outcome worth ruling out everywhere.
     */
    if (*base == '\0')
        base = filename;

    /*
     * The LAST dot, so "OPL.v1.2.ELF" loses only ".ELF". A leading dot
     * is not an extension marker - a file called ".ELF" has that as its
     * whole name, and stripping it would leave nothing to display.
     */
    dot = NULL;
    for (p = base; *p; p++) {
        if (*p == '.' && p != base)
            dot = p;
    }

    n = dot ? (int)(dot - base) : (int)strlen(base);

    /*
     * Nothing left after stripping means the input was all extension.
     * Show the filename itself rather than an empty row the user cannot
     * identify.
     */
    if (n == 0)
        n = (int)strlen(base);

    if (n > size - 1)
        n = size - 1;

    for (i = 0; i < n; i++) {
        char c = base[i];
        out[i] = (c == '_' || c == '-') ? ' ' : c;
    }

    out[n] = '\0';
    return ATLAS_OK;
}
