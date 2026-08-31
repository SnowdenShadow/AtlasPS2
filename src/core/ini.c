/*
 * AtlasPS2 - ini.c
 * A tolerant INI reader.
 */
#include <string.h>

#include "atlas/ini.h"

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/**
 * Copy [start, end) into `out`, trimming surrounding whitespace.
 *
 * @return the length written, or -1 if it does not fit. Refusing beats
 *         truncating: these values become filenames, and a shortened
 *         one names a different file.
 */
static int copy_trimmed(const char *start, const char *end,
                        char *out, int cap, int fold)
{
    int n, i;

    while (start < end && is_space(*start))
        start++;
    while (end > start && is_space(end[-1]))
        end--;

    n = (int)(end - start);
    if (n >= cap)
        return -1;

    for (i = 0; i < n; i++)
        out[i] = fold ? lower(start[i]) : start[i];

    out[n] = '\0';
    return n;
}

atlas_err_t atlas_ini_parse(const char *text, int len,
                            atlas_ini_cb cb, void *user, int *bad_lines)
{
    char section[ATLAS_INI_SECTION_MAX];
    char key[ATLAS_INI_KEY_MAX];
    char value[ATLAS_INI_VALUE_MAX];
    const char *p, *limit;
    int bad = 0;

    if (bad_lines)
        *bad_lines = 0;

    if (!text || !cb || len < 0)
        return ATLAS_EINVAL;

    section[0] = '\0';
    p = text;
    limit = text + len;

    while (p < limit) {
        const char *eol, *body_end, *eq;

        /* A line ends at LF or at the end of the buffer; CR is trimmed
         * later as whitespace, so a CRLF file needs no special case. */
        eol = p;
        while (eol < limit && *eol != '\n')
            eol++;

        body_end = eol;

        /* Leading whitespace: an indented key is still a key. */
        while (p < body_end && is_space(*p))
            p++;

        if (p == body_end || *p == '#' || *p == ';')
            goto next_line;

        if (*p == '[') {
            const char *close = p + 1;

            while (close < body_end && *close != ']')
                close++;

            /*
             * A missing ']' is accepted: the rest of the line is the
             * name. A user who forgot the bracket meant a section, and
             * treating the line as a broken key instead would put every
             * following key in the wrong section - a far worse outcome
             * than guessing right here.
             */
            if (copy_trimmed(p + 1, close, section,
                             ATLAS_INI_SECTION_MAX, 1) < 0) {
                /* Too long to store. Everything under it would be
                 * attributed to the previous section, so drop to the
                 * unnamed one instead of lying about where keys live. */
                section[0] = '\0';
                bad++;
            }
            goto next_line;
        }

        eq = p;
        while (eq < body_end && *eq != '=')
            eq++;

        if (eq == body_end) {  /* no '=': not a key at all */
            bad++;
            goto next_line;
        }

        /* Only the first '=' splits, so a value may contain more. */
        if (copy_trimmed(p, eq, key, ATLAS_INI_KEY_MAX, 1) <= 0) {
            bad++;   /* empty or oversized key */
            goto next_line;
        }

        if (copy_trimmed(eq + 1, body_end, value,
                         ATLAS_INI_VALUE_MAX, 0) < 0) {
            bad++;
            goto next_line;
        }

        if (cb(user, section, key, value) != 0)
            break;

    next_line:
        p = (eol < limit) ? eol + 1 : limit;
    }

    if (bad_lines)
        *bad_lines = bad;

    return ATLAS_OK;
}
