/*
 * AtlasPS2 - test_lz4.c
 *
 * The decoder that every sector of a compressed image passes through.
 *
 * Two things are being pinned here, and they matter for different
 * reasons.
 *
 * The first is that this agrees with the rest of the world. The vectors
 * below were produced by the reference LZ4 compressor, so decoding them
 * to the exact original bytes proves interoperability rather than
 * self-consistency - a decoder that merely round-trips against its own
 * encoder can be uniformly wrong and never notice.
 *
 * The second is that malformed input cannot get out of its buffer. A
 * ZSO comes off a user's drive from somewhere on the internet, and
 * every length and offset in the stream is a number that file chose.
 * The checks below hand it the numbers an attacker would: an offset
 * pointing before the buffer, a length running past the end, a count
 * built from a long run of 255s, a stream that stops mid-token. On this
 * console there is no memory protection - a decoder that trusts those
 * numbers writes over whatever is next in RAM.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/image.h"

#include "lz4_vectors.h"

/* ------------------------------------------------------------------ */
/* Interoperability                                                    */
/* ------------------------------------------------------------------ */

static void check_reference_vectors(void)
{
    unsigned char out[4096];
    int i;

    /*
     * Every vector came out of the reference compressor, so decoding
     * one to the exact original bytes is evidence about agreeing with
     * the rest of the world - not about agreeing with ourselves.
     *
     * They cover the shapes a real ZSO contains: long zero runs, an
     * incompressible sector stored as literals, and matches that
     * overlap their own output.
     */
    for (i = 0; i < (int)(sizeof(k_lz4_vectors) / sizeof(k_lz4_vectors[0]));
         i++) {
        int n;

        assert(k_lz4_vectors[i].raw_len <= (int)sizeof(out));

        memset(out, 0x5A, sizeof(out));

        n = atlas_lz4_decompress(k_lz4_vectors[i].comp,
                                 k_lz4_vectors[i].comp_len,
                                 out, sizeof(out));

        if (n != k_lz4_vectors[i].raw_len) {
            printf("test_lz4: %s decoded %d bytes, expected %d\n",
                   k_lz4_vectors[i].name, n, k_lz4_vectors[i].raw_len);
            assert(0);
        }

        if (memcmp(out, k_lz4_vectors[i].raw,
                   (size_t)k_lz4_vectors[i].raw_len) != 0) {
            printf("test_lz4: %s decoded to the wrong bytes\n",
                   k_lz4_vectors[i].name);
            assert(0);
        }

        /* Nothing past the decoded length was touched. A decoder that
         * overruns by a few bytes passes every check above. */
        assert(out[k_lz4_vectors[i].raw_len] == 0x5A);
    }

    /* An empty block is nothing, not a failure: a run of zero sectors
     * is a legitimate thing for an image to contain. */
    assert(atlas_lz4_decompress("", 0, out, sizeof(out)) == 0);
}

/* ------------------------------------------------------------------ */
/* Malformed input                                                     */
/*                                                                     */
/* Each of these is a number an image could contain, and each would    */
/* read or write outside a buffer if believed. There is no memory      */
/* protection on this console, so "would crash" is optimistic: the     */
/* realistic outcome is silent corruption of whatever is next in RAM.  */
/* ------------------------------------------------------------------ */

static void check_malformed(void)
{
    unsigned char out[64];

    /* A literal count reaching past the end of the input. */
    {
        const unsigned char in[] = { 0xF0, 200, 'x' };
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /* A literal count that fits the input but not the output. */
    {
        unsigned char in[80];
        memset(in, 'x', sizeof(in));
        in[0] = 0xF0;
        in[1] = 55;   /* 70 literals into a 64-byte buffer */
        assert(atlas_lz4_decompress(in, (int)sizeof(in), out, 64) < 0);
    }

    /* An offset pointing before the start of the output buffer. This
     * is the read primitive: believed, it copies from whatever the
     * allocator put in front of this array. */
    {
        const unsigned char in[] = {
            0x10, 'a',
            0x40, 0x00          /* offset 64, but only 1 byte decoded  */
        };
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /* Offset zero is invalid by the format and would loop forever
     * copying a byte onto itself. */
    {
        const unsigned char in[] = { 0x10, 'a', 0x00, 0x00, 0x00 };
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /* A match length running past the end of the output. */
    {
        const unsigned char in[] = {
            0x1F, 'a',
            0x01, 0x00,
            0xFF, 0xFF, 0x01    /* a very long match into 64 bytes     */
        };
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /* A stream that stops in the middle of an offset. */
    {
        const unsigned char in[] = { 0x10, 'a', 0x01 };
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /* A stream that stops in the middle of a length continuation: the
     * count byte says "more follows" and nothing does. */
    {
        const unsigned char in[] = { 0xF0, 0xFF };
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /*
     * A long run of 255s. Each adds to a running total, and without a
     * cap the total overflows into a negative int - which every "is
     * this too long" check below would then pass, because a negative
     * length is less than any buffer size.
     */
    {
        unsigned char in[4096];
        memset(in, 0xFF, sizeof(in));
        in[0] = 0xF0;
        assert(atlas_lz4_decompress(in, (int)sizeof(in),
                                    out, sizeof(out)) < 0);
    }

    /* Bad arguments, including a zero-sized output: a caller that got
     * its sizes wrong must not be handed a write of any length. */
    assert(atlas_lz4_decompress(NULL, 4, out, sizeof(out)) < 0);
    assert(atlas_lz4_decompress("xxxx", 4, NULL, sizeof(out)) < 0);
    assert(atlas_lz4_decompress("xxxx", -1, out, sizeof(out)) < 0);
    assert(atlas_lz4_decompress("xxxx", 4, out, -1) < 0);
    {
        const unsigned char in[] = { 0x10, 'a' };
        assert(atlas_lz4_decompress(in, (int)sizeof(in), out, 0) < 0);
    }
}

/* ------------------------------------------------------------------ */
/* Exact fit                                                           */
/* ------------------------------------------------------------------ */

static void check_exact_fit(void)
{
    unsigned char out[5];

    /*
     * Output that lands exactly on the end of the buffer must succeed.
     * A bounds check written with the wrong comparison rejects this,
     * and the failure mode is a whole class of images that decode fine
     * until the one block that happens to fill its buffer exactly.
     */
    {
        const unsigned char in[] = { 0x50, 'H', 'e', 'l', 'l', 'o' };
        assert(atlas_lz4_decompress(in, (int)sizeof(in), out, 5) == 5);
        assert(memcmp(out, "Hello", 5) == 0);
    }

    /* One byte more than fits is refused. */
    {
        const unsigned char in[] = { 0x60, 'H', 'e', 'l', 'l', 'o', '!' };
        assert(atlas_lz4_decompress(in, (int)sizeof(in), out, 5) < 0);
    }
}

int main(void)
{
    check_reference_vectors();
    check_malformed();
    check_exact_fit();

    printf("test_lz4: OK\n");
    return 0;
}
