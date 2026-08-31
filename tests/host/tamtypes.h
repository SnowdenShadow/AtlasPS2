/*
 * AtlasPS2 - tests/host/tamtypes.h
 *
 * A stand-in for the PS2SDK header of the same name, so the host-side
 * self-checks can include atlas.h without the EE toolchain.
 *
 * Only the fixed-width integer names are declared - the ones atlas.h
 * and the modules under test actually use. Nothing here models the EE:
 * no 128-bit types, no volatile register aliases. A module that needs
 * those is hardware-bound and is checked on the console instead.
 */
#ifndef ATLAS_HOST_TAMTYPES_H
#define ATLAS_HOST_TAMTYPES_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

#endif /* ATLAS_HOST_TAMTYPES_H */
