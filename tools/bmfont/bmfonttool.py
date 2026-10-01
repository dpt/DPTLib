#!/usr/bin/env python3
"""bmfonttool.py -- recompute advance widths in a DPTLib bmfont PNG from
measured glyph ink bounding boxes.

DPTLib's framebuf/bmfont loader (libraries/framebuf/bmfont/bmfont.c) takes a
glyph's advance width from an explicit "advance strip" row baked into the
font PNG (see tools/bmfont/ttf2bmfont.py's docstring for the full format), rather
than deriving it from the ink itself. That strip can drift out of step with
the ink -- e.g. hand-edited in an image editor, or generated with a fixed
inter-glyph gap that turns out wider than intended -- leaving dead space
after (or clipping room before) a glyph.

This tool re-derives each glyph's advance width from its actual ink bounding
box (rightmost ink pixel + 1, relative to the cell's left edge) and rewrites
the advance strip to match, warning about every glyph it changes. Glyphs with
no ink (e.g. space) are left untouched -- there is nothing to measure.

Usage:
    python3 tools/bmfont/bmfonttool.py FONT.png [--out OUT.png] [--dry-run]
"""

import argparse
import struct
import sys
import zlib

CHARS_PER_ROW = 32

IDX_BG = 0
IDX_INK = 1
IDX_ADW = 2
IDX_GRID = 3


def read_png(path):
    """Return (indices, w, h, palette) for a 2bpp colour-type-3 PNG.

    indices is a flat bytearray of length w*h, one palette index per pixel.
    """
    with open(path, "rb") as f:
        data = f.read()

    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit("bmfonttool: %s is not a PNG" % path)

    w = h = bitdepth = colourtype = interlace = None
    palette = None
    idat = bytearray()

    pos = 8
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            w, h, bitdepth, colourtype, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk)
        elif tag == b"PLTE":
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif tag == b"IDAT":
            idat.extend(chunk)
        elif tag == b"IEND":
            break
        pos += 8 + length + 4

    if colourtype != 3 or bitdepth != 2:
        sys.exit("bmfonttool: %s is not a 2bpp paletted bmfont PNG "
                 "(colourtype=%s bitdepth=%s)" % (path, colourtype, bitdepth))
    if interlace:
        sys.exit("bmfonttool: %s is interlaced, unsupported" % path)

    raw = zlib.decompress(bytes(idat))
    rowbytes = (w * 2 + 7) // 8
    indices = bytearray(w * h)
    prev = bytearray(rowbytes)
    pos = 0
    for y in range(h):
        filt = raw[pos]
        pos += 1
        row = bytearray(raw[pos:pos + rowbytes])
        pos += rowbytes
        unfilter_row(filt, row, prev)
        for x in range(w):
            indices[y * w + x] = (row[x >> 2] >> (6 - 2 * (x & 3))) & 3
        prev = row

    return indices, w, h, palette


def unfilter_row(filt, row, prev):
    """Undo a PNG scanline filter in place; bpp is 1 byte (sub-byte pixels)."""
    if filt == 0:  # None
        return
    if filt == 1:  # Sub
        for i in range(1, len(row)):
            row[i] = (row[i] + row[i - 1]) & 0xFF
    elif filt == 2:  # Up
        for i in range(len(row)):
            row[i] = (row[i] + prev[i]) & 0xFF
    elif filt == 3:  # Average
        for i in range(len(row)):
            left = row[i - 1] if i > 0 else 0
            row[i] = (row[i] + (left + prev[i]) // 2) & 0xFF
    elif filt == 4:  # Paeth
        for i in range(len(row)):
            left = row[i - 1] if i > 0 else 0
            up = prev[i]
            upleft = prev[i - 1] if i > 0 else 0
            row[i] = (row[i] + paeth(left, up, upleft)) & 0xFF
    else:
        sys.exit("bmfonttool: unsupported PNG filter type %d" % filt)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def write_png(path, indices, w, h, palette):
    """Write a 2bpp, colour-type-3, non-interlaced PNG (filter type 0)."""
    def chunk(tag, chunkdata):
        return (struct.pack(">I", len(chunkdata)) + tag + chunkdata +
                struct.pack(">I", zlib.crc32(tag + chunkdata) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 2, 3, 0, 0, 0)
    plte = b"".join(struct.pack("BBB", *rgb) for rgb in palette)

    raw = bytearray()
    rowbytes = (w * 2 + 7) // 8
    for y in range(h):
        raw.append(0)
        row = bytearray(rowbytes)
        base = y * w
        for x in range(w):
            row[x >> 2] |= (indices[base + x] & 3) << (6 - 2 * (x & 3))
        raw.extend(row)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"PLTE", plte))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def detect_gridheight(indices, w, h):
    """Port of bmfont.c's detect_gridheight: the shortest cell height whose
    implied strip/glyph rows are self-consistent across the whole image."""
    gridwidth = w // CHARS_PER_ROW

    def row_has(y, value):
        base = y * w
        return any(indices[base + x] == value for x in range(w))

    for gh in range(2, gridwidth * 3 + 1):
        if gh > h or h % gh:
            continue
        ok = True
        for y in range(h):
            has_adw = row_has(y, IDX_ADW)
            has_ink = row_has(y, IDX_INK)
            if y % gh == 0:
                ok = has_adw and not has_ink
            else:
                ok = not has_adw
            if not ok:
                break
        if ok:
            return gh

    sys.exit("bmfonttool: can't determine grid cell height")


def measure_and_fix(indices, w, h, gridheight, dry_run):
    """Recompute each glyph's advance from ink and rewrite the strip row.

    Returns the number of glyphs whose advance width changed.
    """
    cell_w = w // CHARS_PER_ROW
    nrows = h // gridheight
    changed = 0

    for r in range(nrows):
        y_strip = r * gridheight
        for c in range(CHARS_PER_ROW):
            gid = r * CHARS_PER_ROW + c
            x0 = c * cell_w

            old_adv = sum(1 for dx in range(cell_w)
                         if indices[y_strip * w + x0 + dx] == IDX_ADW)

            ink_right = 0
            for dy in range(1, gridheight):
                row = (y_strip + dy) * w
                for dx in range(cell_w):
                    if indices[row + x0 + dx] == IDX_INK:
                        ink_right = max(ink_right, dx + 1)

            if ink_right == 0:
                continue  # no ink (e.g. space) -- nothing to measure

            if ink_right == old_adv:
                continue

            cp = 0x20 + gid
            ch = chr(cp) if 0x20 <= cp < 0x7F else "?"
            sys.stderr.write(
                "bmfonttool: warning: glyph %d (U+%04X %r) advance %d -> %d\n"
                % (gid, cp, ch, old_adv, ink_right))
            changed += 1

            if not dry_run:
                base = y_strip * w + x0
                for dx in range(cell_w):
                    indices[base + dx] = IDX_ADW if dx < ink_right else IDX_BG

    return changed


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("font", help="bmfont PNG to check/fix")
    p.add_argument("--out", help="write the fixed PNG here (default: overwrite "
                                 "the input)")
    p.add_argument("--dry-run", action="store_true",
                   help="report changes without writing the PNG")
    args = p.parse_args(argv)

    indices, w, h, palette = read_png(args.font)
    gridheight = detect_gridheight(indices, w, h)

    changed = measure_and_fix(indices, w, h, gridheight, args.dry_run)

    if changed == 0:
        print("bmfonttool: %s: all advance widths already match their ink" %
              args.font)
        return

    print("bmfonttool: %s: %d glyph(s) had their advance width corrected" %
         (args.font, changed))

    if not args.dry_run:
        out = args.out or args.font
        write_png(out, indices, w, h, palette)
        print("bmfonttool: wrote %s" % out)


if __name__ == "__main__":
    main(sys.argv[1:])
