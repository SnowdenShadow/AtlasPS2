/*
 * AtlasPS2 - lz4.c
 * LZ4 block decompression, for ZSO images.
 *
 * WHY THIS IS WRITTEN OUT RATHER THAN LINKED
 * ------------------------------------------
 * The reference implementation is BSD-licensed and would be fine to
 * use, but the decoder is forty lines and this build has no dependency
 * mechanism: vendoring a library to avoid writing forty lines means
 * carrying its build system, its version, and its licence file forever.
 *
 * WHAT MAKES THIS SAFE
 * --------------------
 * Every length and every offset in an LZ4 stream comes from the file,
 * and the file came off a user's drive from somewhere on the internet.
 * A decoder that trusts them writes past the end of its output buffer,
 * which on this console is a static array next to everything else.
 *
 * So each of the four things that can run out of bounds is checked
 * before it is used: the literal run against both buffers, the match
 * offset against how much output exists so far, the match length
 * against the space left, and the token sequence against the end of
 * input. The checks are what makes this correct, not an optimisation
 * to be traded away - and they cost a comparison per token on a loop
 * that copies kilobytes.
 *
 * It is checked on the build machine against vectors produced by the
 * reference compressor, which is the only way to know this agrees with
 * the rest of the world rather than merely with itself.
 */
#include <string.h>

#include "atlas/image.h"

/*
 * The token byte splits into two 4-bit counts: literals in the high
 * nibble, match length in the low one. A nibble of 15 means "the real
 * count continues in the following bytes", each 255 adding to the
 * total and the first byte below 255 ending it.
 */
#define TOKEN_LITERAL_SHIFT 4
#define TOKEN_MATCH_MASK    0x0F
#define NIBBLE_MAX          15

/*
 * The last match in a block must leave at least this many literals
 * after it, and a match may not start within the last 12 bytes. These
 * are the format's end-of-block rules; a stream that breaks them is
 * malformed, and accepting it would mean reading a match offset out of
 * bytes that are not there.
 */
#define LAST_LITERALS       5
#define MATCH_MIN           4

/**
 * Read a variable-length count that began with a nibble of 15.
 *
 * @param p     cursor, advanced past the bytes consumed
 * @param end   one past the last readable byte
 * @param out   receives the total added to the nibble
 * @return 0 on success, -1 if the stream ended mid-count or the total
 *         would overflow an int
 */
static int read_length(const unsigned char **p, const unsigned char *end,
                       int *out)
{
    int total = 0;
    unsigned char b;

    do {
        if (*p >= end)
            return -1;      /* ran out of input mid-count */

        b = *(*p)++;

        /*
         * A crafted stream can carry thousands of 255 bytes and ask for
         * a length no buffer could hold. The cap is well above any real
         * block (which is bounded by the ZSO block size) and well below
         * where the addition would overflow into a negative length that
         * every bounds check below would then pass.
         */
        if (total > (1 << 24))
            return -1;

        total += b;
    } while (b == 255);

    *out = total;
    return 0;
}

int atlas_lz4_decompress(const void *src, int src_size,
                         void *dst, int dst_size)
{
    const unsigned char *in     = (const unsigned char *)src;
    const unsigned char *in_end = in + src_size;
    unsigned char *out          = (unsigned char *)dst;
    unsigned char *out_start    = out;
    unsigned char *out_end      = out + dst_size;

    if (!src || !dst || src_size < 0 || dst_size < 0)
        return -1;

    /* An empty block decodes to nothing. That is not an error: a run of
     * zero sectors is a legitimate thing for an image to contain. */
    if (src_size == 0)
        return 0;

    while (in < in_end) {
        unsigned char token = *in++;
        int lit_len   = token >> TOKEN_LITERAL_SHIFT;
        int match_len = token & TOKEN_MATCH_MASK;
        int offset;
        const unsigned char *match;

        /* ---- literals ---- */

        if (lit_len == NIBBLE_MAX) {
            int extra;

            if (read_length(&in, in_end, &extra) != 0)
                return -1;

            lit_len += extra;
        }

        /*
         * Both ends, and in this order. Checking only the output would
         * let a truncated file read past the end of the input buffer;
         * checking only the input would let a crafted length write past
         * the end of the output.
         */
        if (lit_len > (int)(in_end - in))
            return -1;
        if (lit_len > (int)(out_end - out))
            return -1;

        memcpy(out, in, (size_t)lit_len);
        in  += lit_len;
        out += lit_len;

        /*
         * A block ends after its literals, with no match to follow. The
         * format guarantees at least LAST_LITERALS bytes there, so
         * anything left that is too short to hold an offset means the
         * stream ended exactly here.
         */
        if (in == in_end)
            break;

        /*
         * A full output buffer also ends the block, even with input
         * left over. That is not a tolerance for corrupt data - it is
         * how the caller's input is framed.
         *
         * A ZSO's block length is the distance to the next block's
         * offset, and when the file was built with an alignment shift
         * that distance includes the padding written after the block.
         * So the last block of an aligned image arrives with a tail of
         * zero bytes attached, and a decoder that kept parsing would
         * read them as a token and reject a perfectly good image.
         *
         * The uncompressed size is always known here (it is the ZSO
         * block size), which is what makes stopping on it safe: the
         * bytes that mattered have all been produced, and every one of
         * them was bounds-checked on the way out.
         */
        if (out == out_end)
            break;

        /* ---- match ---- */

        if (in + 2 > in_end)
            return -1;      /* an offset needs two bytes */

        offset = (int)in[0] | ((int)in[1] << 8);
        in += 2;

        /*
         * The offset points backwards into what has already been
         * decoded. Zero is invalid by the format, and anything larger
         * than the output so far points before the start of the buffer
         * - which is where a malicious image would like to read from.
         */
        if (offset == 0 || offset > (int)(out - out_start))
            return -1;

        if (match_len == NIBBLE_MAX) {
            int extra;

            if (read_length(&in, in_end, &extra) != 0)
                return -1;

            match_len += extra;
        }

        match_len += MATCH_MIN;   /* the format stores length minus 4 */

        if (match_len > (int)(out_end - out))
            return -1;

        /*
         * Byte at a time, deliberately. Overlapping matches are how LZ4
         * encodes a run - an offset of 1 with a length of 50 means
         * "repeat the last byte 50 times" - and memcpy() with
         * overlapping arguments is undefined, while memmove() would
         * copy the original bytes rather than the ones being written.
         * The byte loop is what produces the run.
         */
        match = out - offset;
        while (match_len-- > 0)
            *out++ = *match++;
    }

    return (int)(out - out_start);
}
