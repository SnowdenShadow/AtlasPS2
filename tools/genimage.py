#!/usr/bin/env python3
"""
AtlasPS2 - build a small ISO9660 image, and the same disc as a ZSO.

    python3 tools/genimage.py <outdir>

Writes test.iso and test.zso, a byte-for-byte identical disc in two
formats. The self-check opens both and asserts every sector matches:
that is the whole point of the pair - a ZSO decode bug shows up as a
difference from the ISO rather than as a plausible-looking wrong answer.

The disc is real ISO9660 with a real SYSTEM.CNF, so the same files also
exercise the identification path. Its content is deliberately mixed:
zero padding (which compresses to almost nothing), a repeating pattern
(which compresses well), and pseudo-random data (which does not, and so
is stored uncompressed - the case a real game disc is mostly made of).

The images are written by the check itself rather than committed: they
are 200 KB of generated data with no history worth keeping, and a
generator that can be re-run is easier to trust than a binary blob.
"""
import os
import random
import struct
import sys

import lz4.block

SECTOR = 2048
SECTORS = 100

LBA_PVD = 16
LBA_TERM = 17
LBA_ROOT = 20
LBA_CNF = 22
LBA_DATA = 30

SYSTEM_CNF = (b"BOOT2 = cdrom0:\\SLUS_209.02;1\r\n"
              b"VER = 1.00\r\n"
              b"VMODE = NTSC\r\n")


def dir_record(name, lba, size, is_dir):
    """One ISO9660 directory record, padded to an even length."""
    n = len(name)
    length = 33 + n
    if length % 2:
        length += 1

    rec = bytearray(length)
    rec[0] = length
    rec[2:6] = struct.pack("<I", lba)      # extent, little-endian half
    rec[6:10] = struct.pack(">I", lba)     # and big-endian
    rec[10:14] = struct.pack("<I", size)
    rec[14:18] = struct.pack(">I", size)
    rec[25] = 0x02 if is_dir else 0x00
    rec[32] = n
    rec[33:33 + n] = name
    return bytes(rec)


def build_iso():
    img = bytearray(SECTOR * SECTORS)

    # Primary volume descriptor.
    pvd = bytearray(SECTOR)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[40:72] = b"ATLAS_TEST_DISC".ljust(32)
    pvd[80:84] = struct.pack("<I", SECTORS)
    pvd[84:88] = struct.pack(">I", SECTORS)
    pvd[128:130] = struct.pack("<H", SECTOR)
    pvd[130:132] = struct.pack(">H", SECTOR)
    root = dir_record(b"\x00", LBA_ROOT, SECTOR, True)
    pvd[156:156 + len(root)] = root
    img[LBA_PVD * SECTOR:(LBA_PVD + 1) * SECTOR] = pvd

    # Volume descriptor set terminator.
    term = bytearray(SECTOR)
    term[0] = 255
    term[1:6] = b"CD001"
    term[6] = 1
    img[LBA_TERM * SECTOR:(LBA_TERM + 1) * SECTOR] = term

    # Root directory: ".", "..", SYSTEM.CNF, and a data file.
    d = bytearray()
    d += dir_record(b"\x00", LBA_ROOT, SECTOR, True)
    d += dir_record(b"\x01", LBA_ROOT, SECTOR, True)
    d += dir_record(b"SYSTEM.CNF;1", LBA_CNF, len(SYSTEM_CNF), False)
    d += dir_record(b"DATA.BIN;1", LBA_DATA, SECTOR * 20, False)
    img[LBA_ROOT * SECTOR:LBA_ROOT * SECTOR + len(d)] = d

    img[LBA_CNF * SECTOR:LBA_CNF * SECTOR + len(SYSTEM_CNF)] = SYSTEM_CNF

    # The data file, in three kinds of content. A ZSO stores each
    # differently, so a decode bug affecting only one still shows up.
    #
    # The last group is genuinely incompressible - a seeded PRNG, not an
    # arithmetic sequence, which LZ4 would find the period of and pack
    # down to nothing. It is there to force the stored-uncompressed path
    # in the writer, which is what most of a real game disc takes.
    rng = random.Random(0xA71A5)

    for i in range(20):
        base = (LBA_DATA + i) * SECTOR
        if i < 7:
            pass                                    # zeros, already
        elif i < 14:
            img[base:base + SECTOR] = (b"\xAB\xCD" * (SECTOR // 2))
        else:
            img[base:base + SECTOR] = bytes(rng.randrange(256)
                                            for _ in range(SECTOR))

    return bytes(img)


def build_zso(raw, block_size=8192, align=0):
    """
    The same disc as a ZSO: a header, an index of block offsets, then
    the blocks. A block whose compressed form is not smaller is stored
    as-is with the high bit set in its index entry - which for the
    pseudo-random sectors above is what happens.
    """
    blocks = (len(raw) + block_size - 1) // block_size

    header = bytearray(24)
    header[0:4] = b"ZISO"
    header[4:8] = struct.pack("<I", 24)
    header[8:16] = struct.pack("<Q", len(raw))
    header[16:20] = struct.pack("<I", block_size)
    header[20] = 1
    header[21] = align

    index = []
    body = bytearray()
    base = 24 + (blocks + 1) * 4

    for i in range(blocks):
        chunk = raw[i * block_size:(i + 1) * block_size]
        comp = lz4.block.compress(chunk, store_size=False,
                                  mode="high_compression")

        offset = base + len(body)

        if align:
            pad = (-offset) % (1 << align)
            body += b"\x00" * pad
            offset += pad
            assert offset % (1 << align) == 0

        if len(comp) < len(chunk):
            index.append((offset >> align, False))
            body += comp
        else:
            index.append((offset >> align, True))
            body += chunk

    # The terminator entry gives the last block its length.
    end = base + len(body)
    if align:
        end += (-end) % (1 << align)
    index.append((end >> align, False))

    out = bytearray(header)
    for off, plain in index:
        out += struct.pack("<I", off | (0x80000000 if plain else 0))
    out += body
    return bytes(out)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(outdir, exist_ok=True)

    raw = build_iso()

    with open(os.path.join(outdir, "test.iso"), "wb") as f:
        f.write(raw)

    with open(os.path.join(outdir, "test.zso"), "wb") as f:
        f.write(build_zso(raw, block_size=8192))

    # A second ZSO with a different block size and a non-zero alignment
    # shift: both are things real tools produce, and both change the
    # index arithmetic the reader has to get right.
    with open(os.path.join(outdir, "test16.zso"), "wb") as f:
        f.write(build_zso(raw, block_size=16384, align=2))

    print("wrote test.iso, test.zso, test16.zso to %s" % outdir)


if __name__ == "__main__":
    main()
