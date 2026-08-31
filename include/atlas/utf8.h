/*
 * AtlasPS2 - utf8.h
 *
 * UTF-8 decoding.
 *
 * Translation files, application names and folder names on a FAT volume
 * all arrive as UTF-8, so French accents are two-byte sequences. The
 * font atlas covers Latin-1, so anything above U+00FF is replaced
 * rather than dropped: a visible '?' says "this character is missing",
 * a dropped one silently shortens the text.
 */
#ifndef ATLAS_UTF8_H
#define ATLAS_UTF8_H

#ifdef __cplusplus
extern "C" {
#endif

/** Substituted for anything the font cannot represent. */
#define ATLAS_UTF8_REPLACEMENT '?'

/**
 * Decode one code point and advance *p past it.
 *
 * Malformed input advances by exactly one byte, so the caller always
 * makes progress. A decoder that could stall would hang the render loop
 * on a corrupt string, which on a console means a dead screen with no
 * way out.
 *
 * @return the code point, or ATLAS_UTF8_REPLACEMENT for anything
 *         malformed or above U+00FF.
 */
int atlas_utf8_next(const char **p);

/** Number of code points in a NUL-terminated UTF-8 string. */
int atlas_utf8_length(const char *s);

/**
 * Back `len` up so it does not split a multi-byte sequence.
 *
 * Truncating a string by bytes - which is what fitting text to a pixel
 * width does - can cut a sequence in half. This moves the cut back to
 * the start of the character.
 *
 * @return the adjusted length, never greater than `len`.
 */
int atlas_utf8_trim(const char *s, int len);

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_UTF8_H */
