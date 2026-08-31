#!/usr/bin/env python3
"""
AtlasPS2 - genfont.py

Bakes a TTF into a bitmap font atlas that the PS2 can upload straight to
GS VRAM, plus a C source file with the glyph metrics.

Why bake at build time instead of rendering on the console: the PS2 has
no font rasteriser, and gsKit's only text helper (gsKit_fontm_*) reads
rom0:FONTM, which is Sony ROM data we must not depend on or redistribute.
Baking a redistributable open font gives us our own asset with no
licensing question and no runtime cost.

Output format
-------------
The atlas is 8-bit alpha (one byte of coverage per pixel), uploaded as
GS_PSM_T8 with a 256-entry greyscale CLUT. That is 1 byte per texel
instead of the 4 an RGBA atlas would need, which matters when the whole
GS has 4 MB of VRAM.

Usage:
    genfont.py <font.ttf> <size> <out_basename> [--name IDENT] [--bold]
"""

import sys
import os
import struct

from PIL import Image, ImageDraw, ImageFont

# Latin-1 printable range: enough for French and English, including the
# accented characters the French translation needs.
FIRST_CHAR = 32
LAST_CHAR = 255

PADDING = 1  # transparent gutter so bilinear sampling cannot bleed


def build_atlas(font_path, px_size):
    font = ImageFont.truetype(font_path, px_size)

    glyphs = []
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        ch = chr(code)
        # getbbox returns None for characters that render to nothing.
        bbox = font.getbbox(ch)
        if bbox is None:
            bbox = (0, 0, 0, 0)
        x0, y0, x1, y1 = bbox
        w = max(0, x1 - x0)
        h = max(0, y1 - y0)
        advance = font.getlength(ch)
        glyphs.append(
            {
                "code": code,
                "ch": ch,
                "w": w,
                "h": h,
                "bx": x0,
                "by": y0,
                "advance": int(round(advance)),
            }
        )

    # Simple shelf packing. The glyph set is small and fixed, so there is
    # no point in anything cleverer; we just need a power-of-two atlas.
    #
    # Every candidate width is tried and the one with the smallest total
    # area wins, rather than the first that merely fits: a narrow atlas
    # packs tall, and a 128x1024 sheet wastes both VRAM and headroom
    # against the GS 1024 px limit compared with the 256x512 that holds
    # the same glyphs.
    best = None
    for atlas_w in (128, 256, 512, 1024):
        shelf_x = PADDING
        shelf_y = PADDING
        shelf_h = 0
        placed = []
        for g in glyphs:
            gw = g["w"] + PADDING
            gh = g["h"] + PADDING
            if gw > atlas_w:
                placed = None
                break
            if shelf_x + gw > atlas_w:
                shelf_x = PADDING
                shelf_y += shelf_h + PADDING
                shelf_h = 0
            placed.append((shelf_x, shelf_y))
            shelf_x += gw
            shelf_h = max(shelf_h, gh)
        if placed is None:
            continue

        # Round the height up to a power of two: the GS wants power-of-two
        # texture dimensions.
        atlas_h = shelf_y + shelf_h + PADDING
        pot_h = 1
        while pot_h < atlas_h:
            pot_h *= 2
        if pot_h > 1024:
            continue

        # Area first, then the shorter sheet: 128x1024 and 256x512 hold
        # the same glyphs in the same bytes, but the tall one sits at the
        # GS height limit for no gain.
        score = (atlas_w * pot_h, pot_h)
        if best is None or score < best[0]:
            best = (score, atlas_w, pot_h, placed)

    if best is None:
        raise SystemExit("font too large to pack into 1024x1024")

    _, atlas_w, atlas_h, placed = best
    for g, (px, py) in zip(glyphs, placed):
        g["x"] = px
        g["y"] = py

    # A glyph that spills past the sheet would sample whatever texels sit
    # beyond it, and the failure looks like a subtly wrong character
    # rather than a crash - cheap to assert, expensive to debug on
    # hardware. Overlap is checked the same way for the same reason.
    occupied = set()
    for g in glyphs:
        assert g["x"] + g["w"] <= atlas_w and g["y"] + g["h"] <= atlas_h, (
            "glyph 0x%02X spills out of the %dx%d atlas" % (g["code"], atlas_w, atlas_h)
        )
        for yy in range(g["y"], g["y"] + g["h"]):
            for xx in range(g["x"], g["x"] + g["w"]):
                assert (xx, yy) not in occupied, (
                    "glyph 0x%02X overlaps another at (%d,%d)" % (g["code"], xx, yy)
                )
                occupied.add((xx, yy))

    img = Image.new("L", (atlas_w, atlas_h), 0)
    draw = ImageDraw.Draw(img)

    for g in glyphs:
        if g["w"] == 0 or g["h"] == 0:
            continue
        # Draw at an offset that cancels the glyph's own bearing, so the
        # blit lands the glyph's top-left corner exactly at (x, y).
        draw.text((g["x"] - g["bx"], g["y"] - g["by"]), g["ch"], fill=255, font=font)

    ascent, descent = font.getmetrics()
    line_height = ascent + descent

    return img, glyphs, ascent, line_height


def emit_c(out_base, ident, img, glyphs, ascent, line_height, src_name, px_size):
    atlas_w, atlas_h = img.size
    data = img.tobytes()

    c_path = out_base + ".c"
    h_path = out_base + ".h"

    guard = "ATLAS_FONT_%s_H" % ident.upper()

    with open(h_path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(" * AtlasPS2 - generated font atlas: %s\n" % ident)
        f.write(" * Source face: %s at %d px\n" % (src_name, px_size))
        f.write(" *\n")
        f.write(" * GENERATED FILE - do not edit. Regenerate with tools/genfont.py.\n")
        f.write(" */\n")
        f.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
        f.write('#include "atlas/font.h"\n\n')
        f.write("extern const atlas_font_data_t atlas_font_%s;\n\n" % ident)
        f.write("#endif /* %s */\n" % guard)

    with open(c_path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(" * AtlasPS2 - generated font atlas: %s\n" % ident)
        f.write(" * Source face: %s at %d px\n" % (src_name, px_size))
        f.write(" *\n")
        f.write(" * GENERATED FILE - do not edit. Regenerate with tools/genfont.py.\n")
        f.write(" */\n")
        f.write('#include "%s"\n\n' % os.path.basename(h_path))

        f.write(
            "static const unsigned char s_%s_pixels[%d] "
            "__attribute__((aligned(16))) = {\n" % (ident, len(data))
        )
        for i in range(0, len(data), 20):
            chunk = data[i : i + 20]
            f.write("    " + ",".join(str(b) for b in chunk) + ",\n")
        f.write("};\n\n")

        f.write("static const atlas_glyph_t s_%s_glyphs[%d] = {\n" % (ident, len(glyphs)))
        for g in glyphs:
            f.write(
                "    {%d,%d,%d,%d,%d,%d,%d}, /* %s */\n"
                % (
                    g["x"],
                    g["y"],
                    g["w"],
                    g["h"],
                    g["bx"],
                    g["by"],
                    g["advance"],
                    ("0x%02X" % g["code"]),
                )
            )
        f.write("};\n\n")

        f.write("const atlas_font_data_t atlas_font_%s = {\n" % ident)
        f.write("    s_%s_pixels,\n" % ident)
        f.write("    %d, %d,\n" % (atlas_w, atlas_h))
        f.write("    s_%s_glyphs,\n" % ident)
        f.write("    %d, %d,\n" % (FIRST_CHAR, LAST_CHAR))
        f.write("    %d, %d\n" % (ascent, line_height))
        f.write("};\n")

    return c_path, h_path, atlas_w, atlas_h


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opts = [a for a in sys.argv[1:] if a.startswith("--")]

    if len(args) < 3:
        print(__doc__)
        return 1

    font_path, px_size, out_base = args[0], int(args[1]), args[2]

    ident = os.path.basename(out_base).replace("-", "_")
    for o in opts:
        if o.startswith("--name="):
            ident = o.split("=", 1)[1]

    img, glyphs, ascent, line_height = build_atlas(font_path, px_size)
    c_path, h_path, w, h = emit_c(
        out_base, ident, img, glyphs, ascent, line_height,
        os.path.basename(font_path), px_size,
    )

    print(
        "%s: %dx%d atlas, %d glyphs, line height %d -> %s"
        % (ident, w, h, len(glyphs), line_height, c_path)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
