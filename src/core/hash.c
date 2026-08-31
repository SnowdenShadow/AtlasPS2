/*
 * AtlasPS2 - hash.c
 * CRC-32, computed a nibble at a time.
 */
#include "atlas/hash.h"

/*
 * A 16-entry table rather than the usual 256-entry one: two lookups per
 * byte instead of one, against 1 KB of the PS2's 32 MB saved and a table
 * small enough to sit in cache next to the data being hashed. On a
 * checksum that runs over a 700 KB ELF while the user waits, staying in
 * cache is worth more than the second lookup costs.
 *
 * The values are the standard reflected CRC-32 table for the low nibble;
 * the self-check pins the result against the published check value for
 * "123456789", so a typo here cannot pass unnoticed.
 */
static const u32 s_table[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

u32 atlas_crc32(u32 crc, const void *data, int len)
{
    const unsigned char *p = (const unsigned char *)data;
    int i;

    if (!data || len <= 0)
        return crc;

    /*
     * The conventional pre- and post-inversion is applied here rather
     * than by the caller, so that chaining works: the value handed back
     * is a real CRC of everything so far, which means a caller can log
     * it or compare it mid-file without knowing about the convention.
     */
    crc = ~crc;

    for (i = 0; i < len; i++) {
        crc ^= p[i];
        crc = (crc >> 4) ^ s_table[crc & 0x0Fu];
        crc = (crc >> 4) ^ s_table[crc & 0x0Fu];
    }

    return ~crc;
}
