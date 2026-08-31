/*
 * AtlasPS2 - test_hash.c
 *
 * The checksum that decides whether a copied BOOT.ELF is good.
 *
 * The important check here is the published one. CRC-32 has a standard
 * check value - 0xCBF43926 for the nine bytes "123456789" - and pinning
 * against it proves this implementation agrees with the rest of the
 * world rather than merely agreeing with itself. A table typo would
 * still produce a self-consistent hash, and the installer would happily
 * verify a copy against a wrong-but-stable value forever.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "atlas/hash.h"

static void check_known_values(void)
{
    /* The IEEE 802.3 check value. Any conforming CRC-32 gives this. */
    assert(atlas_crc32(ATLAS_CRC32_INIT, "123456789", 9) == 0xCBF43926u);

    /* Nothing hashes to the initial value: an empty file has an empty
     * checksum, so an installer comparing a zero-byte copy against a
     * real file cannot accidentally match. */
    assert(atlas_crc32(ATLAS_CRC32_INIT, "", 0) == ATLAS_CRC32_INIT);

    assert(atlas_crc32(ATLAS_CRC32_INIT, "a", 1) == 0xE8B7BE43u);
    assert(atlas_crc32(ATLAS_CRC32_INIT, "abc", 3) == 0x352441C2u);
}

static void check_chaining(void)
{
    const char *text = "The quick brown fox jumps over the lazy dog";
    int len = (int)strlen(text);
    u32 whole, split;
    int cut;

    /*
     * Files are hashed in 32 KB chunks, so a chained call has to give
     * the same answer as one pass over the whole buffer. If it did not,
     * verification would fail on every file larger than one chunk -
     * which is every ELF this ever runs on.
     */
    whole = atlas_crc32(ATLAS_CRC32_INIT, text, len);

    for (cut = 0; cut <= len; cut++) {
        split = atlas_crc32(ATLAS_CRC32_INIT, text, cut);
        split = atlas_crc32(split, text + cut, len - cut);
        assert(split == whole);
    }
}

static void check_detects_corruption(void)
{
    unsigned char buf[512];
    u32 good, bad;
    int i;

    for (i = 0; i < (int)sizeof(buf); i++)
        buf[i] = (unsigned char)(i * 7 + 3);

    good = atlas_crc32(ATLAS_CRC32_INIT, buf, (int)sizeof(buf));

    /* A single flipped bit anywhere must change the result. This is the
     * failure being defended against: a Memory Card sector that returns
     * almost the right bytes. */
    for (i = 0; i < (int)sizeof(buf); i++) {
        buf[i] ^= 0x01;
        bad = atlas_crc32(ATLAS_CRC32_INIT, buf, (int)sizeof(buf));
        assert(bad != good);
        buf[i] ^= 0x01;
    }

    /* Two files of the same length differing only in order must differ:
     * a checksum insensitive to position would accept a scrambled copy. */
    assert(atlas_crc32(ATLAS_CRC32_INIT, "ab", 2)
           != atlas_crc32(ATLAS_CRC32_INIT, "ba", 2));
}

static void check_bad_arguments(void)
{
    /* A caller that hands over nothing gets its running value back
     * unchanged, so a read loop that ends on a zero-length final chunk
     * does not corrupt the checksum it spent a whole file building. */
    assert(atlas_crc32(0x12345678u, NULL, 10) == 0x12345678u);
    assert(atlas_crc32(0x12345678u, "x", 0) == 0x12345678u);
    assert(atlas_crc32(0x12345678u, "x", -1) == 0x12345678u);
}

int main(void)
{
    check_known_values();
    check_chaining();
    check_detects_corruption();
    check_bad_arguments();

    printf("test_hash: all checks passed\n");
    return 0;
}
