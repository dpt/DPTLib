#!/usr/bin/env python3
"""ttf2bmfont.py -- render a pixel-design TTF into a DPTLib bmfont PNG.

DPTLib's framebuf/bmfont loader (libraries/framebuf/bmfont/bmfont.c) reads
proportional bitmap fonts from a very specific 2bpp paletted PNG:

  * 4-entry palette, order is the contract:
      0 = background, 1 = glyph ink, 2 = advance-width marker, 3 = grid
    (the left-sidebearing line index 3 draws is a visual aid only, but its
    baseline and cell-bottom rows are load-bearing -- see below).
  * exactly 32 glyph cells per row, packed edge to edge; cellW = pngW / 32,
    and cellW must be <= 16.
  * the top row of every cell is the "advance strip": it holds `advance`
    index-2 pixels (left-justified here) and no index-1 pixels. The loader
    counts index-2 pixels in the cell to get the advance width.
  * the remaining cellH-1 rows are the glyph bitmap; index-1 pixels are ink.
  * cell 0 is U+0020 (space); codepoints run contiguously upward. There is no
    character map and no stored first-codepoint.
  * the loader derives the font's ascent and descent from cell 0 (space,
    which carries no ink to interfere): the first two full-width rows of
    index-3 pixels within the cell body are the baseline and the cell
    bottom -- ascent is the baseline's offset from the cell top, descent is
    the cell bottom's offset from the baseline. This script draws those rows
    at `ascent` and `char_h - 1` (see draw_grid below). --no-grid omits them
    along with the other grid lines, and the loader then falls back to no
    descender space for that font.

Requires freetype-py ( pip install freetype-py ). PNG is written by hand so
Pillow is not needed.

Usage:
    python3 tools/ttf2bmfont.py FONT.ttf OUT.png --size PX [options]
"""

import argparse
import struct
import sys
import zlib

import freetype

# Palette matches resources/bmfonts/Nokia.png and Digits-Bold.png.
PALETTE = [(255, 255, 255), (0, 0, 0), (0, 0, 255), (192, 192, 192)]

IDX_BG = 0
IDX_INK = 1
IDX_ADW = 2
IDX_GRID = 3

CHARS_PER_ROW = 32
MAX_CELL_W = 16


def load_glyphs(path, size, first, last):
    """Return (list of glyph dicts, face) for codepoints first..last inclusive.

    Each glyph dict has: cp, bits (list of (x, y) set pixels, origin at the
    pen position, y down), left, top, width, rows, advance.
    """
    face = freetype.Face(path)
    face.set_pixel_sizes(0, size)

    glyphs = []
    for cp in range(first, last + 1):
        face.load_char(chr(cp),
                       freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
        slot = face.glyph
        bm = slot.bitmap
        bits = []
        for by in range(bm.rows):
            rowbase = by * bm.pitch
            for bx in range(bm.width):
                byte = bm.buffer[rowbase + (bx >> 3)]
                if byte & (0x80 >> (bx & 7)):
                    bits.append((bx, by))
        glyphs.append(dict(cp=cp,
                           bits=bits,
                           left=slot.bitmap_left,
                           top=slot.bitmap_top,
                           width=bm.width,
                           rows=bm.rows,
                           advance=slot.advance.x >> 6))
    return glyphs, face


def measure_cell(glyphs, pad, advance_source):
    """Work out the common cell geometry from the rendered glyphs.

    Returns (cellW, cellH, charH, ascent, minLeft).
    """
    ascent = 0
    descent = 0
    ink_right = 0
    min_left = 0
    for g in glyphs:
        if g["bits"]:
            ascent = max(ascent, g["top"])
            descent = max(descent, g["rows"] - g["top"])
            ink_right = max(ink_right, g["left"] + g["width"])
            min_left = min(min_left, g["left"])

    max_advance = 0
    for g in glyphs:
        adv = g["advance"] if advance_source == "ttf" else _ink_advance(g)
        max_advance = max(max_advance, adv)

    cell_w = max(ink_right - min_left, max_advance) + pad
    char_h = ascent + descent + pad
    cell_h = char_h + 1  # + strip row

    if cell_w < 1:
        cell_w = 1
    if cell_w > MAX_CELL_W:
        sys.exit("ttf2bmfont: cell width %d exceeds the loader limit of %d; "
                 "use a smaller --size" % (cell_w, MAX_CELL_W))

    return cell_w, cell_h, char_h, ascent, min_left


def _ink_advance(g):
    if not g["bits"]:
        return 0
    return g["left"] + g["width"] + 1


def advance_for(g, cell_w, advance_source):
    """Clamp the chosen advance into [1, cell_w].

    At least 1 so every strip row carries an index-2 pixel (the loader's
    height detector requires it).
    """
    adv = g["advance"] if advance_source == "ttf" else _ink_advance(g)
    if adv < 1:
        adv = 1
    if adv > cell_w:
        adv = cell_w
    return adv


def build_indices(glyphs, cell_w, cell_h, char_h, ascent, min_left,
                  advance_source, draw_grid):
    """Render every glyph into an H*W buffer of palette indices."""
    nrows = (len(glyphs) + CHARS_PER_ROW - 1) // CHARS_PER_ROW
    w = CHARS_PER_ROW * cell_w
    h = nrows * cell_h
    buf = bytearray(w * h)  # zero == IDX_BG

    def put(x, y, idx):
        if 0 <= x < w and 0 <= y < h:
            buf[y * w + x] = idx

    baseline = ascent  # body-row index of the baseline

    if draw_grid:
        for r in range(nrows):
            body0 = r * cell_h + 1
            for c in range(CHARS_PER_ROW):
                x0 = c * cell_w
                for dy in range(char_h):  # left sidebearing line
                    put(x0, body0 + dy, IDX_GRID)
                for dx in range(cell_w):  # baseline and cell-bottom lines
                    put(x0 + dx, body0 + baseline, IDX_GRID)
                    put(x0 + dx, body0 + char_h - 1, IDX_GRID)

    warned = [False]
    advances = []
    for i, g in enumerate(glyphs):
        r, c = divmod(i, CHARS_PER_ROW)
        x0 = c * cell_w
        y_strip = r * cell_h
        body0 = y_strip + 1

        adv = advance_for(g, cell_w, advance_source)
        advances.append(adv)
        for dx in range(adv):  # advance strip
            put(x0 + dx, y_strip, IDX_ADW)

        for bx, by in g["bits"]:  # glyph ink (wins over any grid pixel)
            gx = x0 + (g["left"] - min_left) + bx
            gy = body0 + (ascent - g["top"]) + by
            if not (x0 <= gx < x0 + cell_w and body0 <= gy < body0 + char_h):
                if not warned[0]:
                    sys.stderr.write("ttf2bmfont: warning: glyph U+%04X "
                                     "overflows its cell; clipping\n" % g["cp"])
                    warned[0] = True
                continue
            put(gx, gy, IDX_INK)

    return buf, w, h, advances


def write_png(path, buf, w, h):
    """Write a 2bpp, colour-type-3, non-interlaced PNG."""
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 2, 3, 0, 0, 0)

    plte = b"".join(struct.pack("BBB", *rgb) for rgb in PALETTE)

    raw = bytearray()
    rowbytes = (w * 2 + 7) // 8
    for y in range(h):
        raw.append(0)  # filter type 0 (none)
        row = bytearray(rowbytes)
        base = y * w
        for x in range(w):
            row[x >> 2] |= (buf[base + x] & 3) << (6 - 2 * (x & 3))
        raw.extend(row)

    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"PLTE", plte))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def selfcheck(buf, w, h, cell_w, cell_h, advances):
    """Re-parse the buffer with the loader's own rules and assert agreement."""
    assert w % CHARS_PER_ROW == 0, "width not a multiple of 32"
    assert w // CHARS_PER_ROW == cell_w <= MAX_CELL_W, "bad cell width"
    assert h % cell_h == 0, "height not a multiple of the cell height"

    def has(idx, y):
        base = y * w
        return any(buf[base + x] == idx for x in range(w))

    for y in range(h):
        if y % cell_h == 0:
            assert has(IDX_ADW, y), "strip row %d has no advance pixel" % y
            assert not has(IDX_INK, y), "strip row %d has ink" % y
        else:
            assert not has(IDX_ADW, y), "body row %d has an advance pixel" % y

    # round-trip the advance counts
    for i, want in enumerate(advances):
        r, c = divmod(i, CHARS_PER_ROW)
        y = r * cell_h
        x0 = c * cell_w
        got = sum(1 for dx in range(cell_w) if buf[y * w + x0 + dx] == IDX_ADW)
        assert got == want, "glyph %d advance %d != %d" % (i, got, want)

    print("selfcheck OK: %dx%d, cell %dx%d, %d glyphs" %
          (w, h, cell_w, cell_h, len(advances)))


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("font")
    p.add_argument("out")
    p.add_argument("--size", type=int, required=True,
                   help="pixel em size passed to FreeType")
    p.add_argument("--pad", type=int, default=1,
                   help="extra pixels added to the glyph bbox for grid lines")
    p.add_argument("--advance-source", choices=("ttf", "ink"), default="ttf",
                   help="'ttf' uses the font's advance, 'ink' uses ink width+1")
    p.add_argument("--no-grid", action="store_true",
                   help="omit the grey guide lines")
    p.add_argument("--first", type=lambda s: int(s, 0), default=0x20)
    p.add_argument("--last", type=lambda s: int(s, 0), default=0x7E)
    p.add_argument("--selfcheck", action="store_true",
                   help="re-parse the result and assert it is well formed")
    args = p.parse_args(argv)

    glyphs, _ = load_glyphs(args.font, args.size, args.first, args.last)

    # pad the cell count up to a multiple of 32 with blank glyphs
    while len(glyphs) % CHARS_PER_ROW:
        glyphs.append(dict(cp=0, bits=[], left=0, top=0, width=0, rows=0,
                           advance=1))

    cell_w, cell_h, char_h, ascent, min_left = measure_cell(
        glyphs, args.pad, args.advance_source)

    buf, w, h, advances = build_indices(glyphs, cell_w, cell_h, char_h,
                                        ascent, min_left, args.advance_source,
                                        not args.no_grid)

    write_png(args.out, buf, w, h)
    print("wrote %s (%dx%d, cell %dx%d, charheight %d, %d glyphs)" %
          (args.out, w, h, cell_w, cell_h, cell_h - 1, len(glyphs)))

    if args.selfcheck:
        selfcheck(buf, w, h, cell_w, cell_h, advances)


if __name__ == "__main__":
    main(sys.argv[1:])
