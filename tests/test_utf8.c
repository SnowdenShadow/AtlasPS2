/*
 * AtlasPS2 - test_utf8.c
 *
 * Self-check for the UTF-8 decoder. Builds and runs on the host with a
 * plain compiler - no PS2SDK, no console - because the properties that
 * matter here (never stall, never read past the terminator, French
 * accents survive) are pure string handling.
 *
 *     make -C tests
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/utf8.h"

#define R ATLAS_UTF8_REPLACEMENT

/* Decode a whole string into `out`; returns the count. */
static int decode_all(const char *s, int *out, int max)
{
    int n = 0;

    while (*s && n < max)
        out[n++] = atlas_utf8_next(&s);

    return n;
}

static void test_ascii(void)
{
    int cp[8];
    int n = decode_all("Hi!", cp, 8);

    assert(n == 3);
    assert(cp[0] == 'H' && cp[1] == 'i' && cp[2] == '!');
    assert(atlas_utf8_length("Hi!") == 3);
}

static void test_french_accents(void)
{
    /* "Éteindre la console" - the menu label that must not break. */
    const char *s = "\xC3\x89teindre";
    int cp[16];
    int n = decode_all(s, cp, 16);

    assert(cp[0] == 0xC9);            /* U+00C9 LATIN CAPITAL E WITH ACUTE */
    assert(n == 8);                   /* 9 bytes, 8 code points */
    assert(atlas_utf8_length(s) == 8);

    /* Every accent the French UI uses is Latin-1, so none is replaced. */
    {
        const char *accents = "\xC3\xA0\xC3\xA2\xC3\xA7\xC3\xA9\xC3\xA8"
                              "\xC3\xAA\xC3\xAB\xC3\xAE\xC3\xAF\xC3\xB4"
                              "\xC3\xB9\xC3\xBB\xC3\xBC";
        int i, m = decode_all(accents, cp, 16);

        assert(m == 13);
        for (i = 0; i < m; i++)
            assert(cp[i] != R && cp[i] > 0x7F);
    }
}

static void test_above_latin1_replaced(void)
{
    /* U+25CF BLACK CIRCLE: three bytes, no glyph in the atlas. */
    const char *s = "\xE2\x97\x8F";
    int cp[4];
    int n = decode_all(s, cp, 4);

    assert(n == 1);        /* consumed as one character, not three */
    assert(cp[0] == R);
}

static void test_malformed_always_advances(void)
{
    /*
     * The property that keeps the render loop alive: whatever the bytes,
     * the pointer moves. A stalling decoder means a dead screen with no
     * way out on a console.
     */
    static const char *bad[] = {
        "\x80",             /* lone continuation                   */
        "\xC3",             /* truncated 2-byte lead at end        */
        "\xE2\x97",         /* truncated 3-byte lead at end        */
        "\xC3\x41",         /* lead followed by a non-continuation */
        "\xFF\xFE",         /* not UTF-8 at all                    */
        "\xE2\x28\xA1",     /* invalid second byte                 */
    };
    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        const char *p = bad[i];
        const char *start = p;
        int guard = 0;

        while (*p) {
            const char *before = p;

            atlas_utf8_next(&p);
            assert(p > before);              /* progress, every time */
            assert(++guard <= 8);
        }

        assert(p == start + strlen(start));  /* never overran the NUL */
    }
}

static void test_trim(void)
{
    const char *s = "\xC3\x89teindre";   /* E-acute occupies bytes 0..1 */

    /* A cut inside the sequence moves back to its start. */
    assert(atlas_utf8_trim(s, 1) == 0);

    /* A cut on a lead byte or ASCII byte is already safe. */
    assert(atlas_utf8_trim(s, 2) == 2);
    assert(atlas_utf8_trim(s, 5) == 5);

    /* Never grows, and copes with the edges. */
    assert(atlas_utf8_trim(s, 0) == 0);
    assert(atlas_utf8_trim(NULL, 4) == 0);
    assert(atlas_utf8_trim(s, -1) == 0);

    /*
     * A trimmed prefix must itself decode cleanly - that is the point of
     * trimming, and it is what atlas_font_draw_clipped() relies on.
     */
    {
        char buf[16];
        int len = atlas_utf8_trim(s, 1);
        int cp[8];

        memcpy(buf, s, (size_t)len);
        buf[len] = '\0';
        assert(decode_all(buf, cp, 8) == 0);
    }
}

static void test_empty(void)
{
    assert(atlas_utf8_length("") == 0);
    assert(atlas_utf8_length(NULL) == 0);
}

int main(void)
{
    test_ascii();
    test_french_accents();
    test_above_latin1_replaced();
    test_malformed_always_advances();
    test_trim();
    test_empty();

    printf("utf8: all checks passed\n");
    return 0;
}
