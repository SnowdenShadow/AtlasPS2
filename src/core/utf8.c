/*
 * AtlasPS2 - utf8.c
 * UTF-8 decoding.
 */
#include "atlas/utf8.h"

int atlas_utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    int cp;

    /*
     * The && short-circuits before s[1] or s[2] is read unless the
     * preceding byte was a valid continuation, and NUL never is. So a
     * truncated sequence at the end of the string falls through to the
     * one-byte case rather than reading past the terminator.
     */
    if (s[0] < 0x80) {
        cp = s[0];
        *p += 1;
    } else if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *p += 2;
    } else if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80
               && (s[2] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *p += 3;
    } else {
        cp = ATLAS_UTF8_REPLACEMENT;
        *p += 1;
    }

    return (cp > 0xFF) ? ATLAS_UTF8_REPLACEMENT : cp;
}

int atlas_utf8_length(const char *s)
{
    int n = 0;

    if (!s)
        return 0;

    while (*s) {
        atlas_utf8_next(&s);
        n++;
    }

    return n;
}

int atlas_utf8_trim(const char *s, int len)
{
    if (!s || len <= 0)
        return 0;

    /* Continuation bytes are 10xxxxxx; back up to the lead byte. */
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80)
        len--;

    return len;
}
