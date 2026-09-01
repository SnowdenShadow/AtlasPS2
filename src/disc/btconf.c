/*
 * AtlasPS2 - btconf.c
 * Filtering the IOP boot module list.
 *
 * Pure text handling over a buffer, deliberately: it is the one part of
 * the drive-emulation handover that can be checked on a build machine,
 * and getting it wrong is an IOP that boots wrong on a console with no
 * screen left. tests/test_btconf.c is that check.
 *
 * It reads no file and knows nothing about rom0:. The caller supplies
 * the bytes.
 */
#include <string.h>

#include "atlas/btconf.h"

/*
 * The modules that own the drive, and the order they are named in.
 *
 * cdvdman is the one we are replacing. cdvdfsv is what the EE's disc
 * calls arrive through, and it imports cdvdman, so it cannot load in
 * the window where cdvdman is missing. cdvdstm is the streaming half
 * and imports cdvdman as well.
 *
 * All three come back from rom0: once our module holds the name, which
 * is why this list is also the order they are recorded in.
 */
static const char *const k_drive[] = {
    "CDVDMAN",
    "CDVDFSV",
    "CDVDSTM",
};

#define K_DRIVE_COUNT ((int)(sizeof(k_drive) / sizeof(k_drive[0])))

/* Every name above has to fit the field it is copied into, and the
 * copy below is a memcpy that would not notice. A name added later that
 * does not fit is a build error here rather than four bytes written
 * past the end of a row. */
#define K_DRIVE_LONGEST 7                   /* strlen("CDVDMAN") */
typedef char k_drive_names_fit[
    (K_DRIVE_LONGEST + 1 <= ATLAS_BTCONF_NAME_MAX) ? 1 : -1];

static int is_blank(char c)
{
    return c == ' ' || c == '\t';
}

static char upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/**
 * Which drive module this line names, or -1.
 *
 * A line names a module when its first token, ignoring leading blanks
 * and case, is exactly that name. "CDVDMAN" matches; "CDVDMAN2" does
 * not, and neither does a line whose first token merely starts with it
 * - a revision carrying a module we have not heard of is a module we
 * leave alone.
 *
 * @param line  start of the line
 * @param len   its length, not counting the terminator
 */
static int drive_module(const char *line, int len)
{
    int start = 0;
    int end;
    int i, j;

    while (start < len && is_blank(line[start]))
        start++;

    end = start;
    while (end < len && !is_blank(line[end]))
        end++;

    if (end == start)
        return -1;

    for (i = 0; i < K_DRIVE_COUNT; i++) {
        const char *name = k_drive[i];
        int n = (int)strlen(name);

        if (end - start != n)
            continue;

        for (j = 0; j < n; j++) {
            if (upper(line[start + j]) != name[j])
                break;
        }

        if (j == n)
            return i;
    }

    return -1;
}

atlas_err_t atlas_btconf_filter(const char *src, int len, atlas_btconf_t *out)
{
    int pos = 0;
    int wrote = 0;
    int found_cdvdman = 0;

    if (!src || !out || len < 0)
        return ATLAS_EINVAL;

    memset(out, 0, sizeof(*out));

    while (pos < len) {
        int eol = pos;
        int next;
        int which;

        /* The line's text, then whatever ends it. Both separators and
         * both orders: this file comes off a ROM, not off a filesystem
         * we control, and a list that ends its lines with CRLF is a
         * list whose last module name would otherwise carry a stray
         * carriage return into the comparison. */
        while (eol < len && src[eol] != '\n' && src[eol] != '\r')
            eol++;

        next = eol;
        if (next < len && src[next] == '\r')
            next++;
        if (next < len && src[next] == '\n')
            next++;

        which = drive_module(src + pos, eol - pos);

        if (which >= 0) {
            found_cdvdman |= (which == 0);

            if (out->removed_count < ATLAS_BTCONF_REMOVED_MAX) {
                /* The canonical name, not the line's spelling: this is
                 * about to be pasted after "rom0:" and handed to the
                 * module loader, and a name that differs from the ROM's
                 * own by a letter of case is a module not found. The
                 * length is guaranteed by k_drive_names_fit above. */
                memcpy(out->removed[out->removed_count], k_drive[which],
                       strlen(k_drive[which]) + 1);
                out->removed_count++;
            }

            pos = next;
            continue;
        }

        if (wrote + (next - pos) > ATLAS_BTCONF_MAX)
            return ATLAS_ENOMEM;

        memcpy(out->text + wrote, src + pos, (size_t)(next - pos));
        wrote += next - pos;
        pos = next;
    }

    /*
     * A list with no cdvdman in it is not a list this understands.
     *
     * It is not a list we could act on either: handing the IOP a
     * filtered copy of a file whose format we guessed wrong is the
     * failure this whole path exists to avoid, and it would happen
     * after the last screen. Refusing is the only answer that can still
     * be seen.
     */
    if (!found_cdvdman)
        return ATLAS_EFORMAT;

    out->len = wrote;
    return ATLAS_OK;
}
