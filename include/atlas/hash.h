/*
 * AtlasPS2 - hash.h
 *
 * CRC-32 (the IEEE 802.3 polynomial, the one zip and PNG use).
 *
 * WHY A CHECKSUM AT ALL
 * ---------------------
 * The installer and the updater both copy an ELF onto a Memory Card and
 * then have to answer one question: is what landed there the same as
 * what was read? A byte count cannot answer it. A Memory Card with a
 * failing sector accepts the write, reports the right length, and hands
 * back different bytes later - and the file in question is the one the
 * console boots. A corrupt BOOT.ELF is exactly the black screen this
 * project exists to prevent, so the copy is verified by reading it back
 * and comparing checksums before anything is renamed into place.
 *
 * WHY CRC-32 AND NOT SOMETHING STRONGER
 * -------------------------------------
 * This defends against a storage fault, not against an attacker, and
 * CRC-32 detects every burst error shorter than 33 bits - which is what
 * a bad sector produces. It also costs one table lookup per byte on a
 * 294 MHz CPU, where a cryptographic hash would make verifying a 700 KB
 * ELF something the user watches happen.
 *
 * Being a standard algorithm matters for the self-check: the test can
 * assert the published value for "123456789" rather than whatever this
 * implementation happens to produce, which would only prove that it
 * agrees with itself.
 */
#ifndef ATLAS_HASH_H
#define ATLAS_HASH_H

#include "atlas/atlas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Update a running CRC-32 with `len` bytes.
 *
 * Start from ATLAS_CRC32_INIT and pass the result back in to continue
 * over a file read in chunks, which is how a 700 KB ELF is checksummed
 * without holding it in RAM.
 */
u32 atlas_crc32(u32 crc, const void *data, int len);

/** The starting value for a chain of atlas_crc32() calls. */
#define ATLAS_CRC32_INIT 0u

#ifdef __cplusplus
}
#endif

#endif /* ATLAS_HASH_H */
