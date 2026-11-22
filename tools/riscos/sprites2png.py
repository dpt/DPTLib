#!/usr/bin/env python3
"""sprites2png.py -- split a RISC OS sprite file into one PNG per sprite.

Handles old-format (mode number) and new-format (sprite type) sprites at
1, 2, 4 and 8 bpp, with or without a palette or mask. Sprites without a
palette use the default Wimp palette, so need 1, 2 or 4 bpp. Pixels are
written as stored: no correction for modes with rectangular pixels. Each
PNG is named after its sprite.

Usage: python3 tools/riscos/sprites2png.py SPRITEFILE OUTDIR
"""

import os
import struct
import sys
import zlib

WIMP16 = [0xFFFFFF, 0xDDDDDD, 0xBBBBBB, 0x999999, 0x777777, 0x555555,
          0x333333, 0x000000, 0x004499, 0xEEEE00, 0x00CC00, 0xDD0000,
          0xEEEEBB, 0x558800, 0xFFBB00, 0x00BBFF]
WIMP4 = [0xFFFFFF, 0xBBBBBB, 0x777777, 0x000000]
WIMP2 = [0xFFFFFF, 0x000000]

# old mode number -> bits per pixel
MODE_BPP = {0: 1, 1: 2, 2: 4, 4: 1, 5: 2, 8: 2, 9: 4, 10: 8, 11: 2,
            12: 4, 13: 8, 14: 4, 15: 8, 16: 4, 17: 4, 18: 1, 19: 2,
            20: 4, 21: 8, 24: 8, 25: 1, 26: 2, 27: 4, 28: 8, 31: 4,
            32: 8, 33: 1, 34: 2, 35: 4, 36: 8, 37: 1, 38: 2, 39: 4, 40: 8}
TYPE_BPP = {1: 1, 2: 2, 3: 4, 4: 8}


def bpp_of(mode):
    if mode < 256:
        if mode not in MODE_BPP:
            raise ValueError("unknown mode %d" % mode)
        return MODE_BPP[mode]
    stype = (mode >> 27) & 0x7F
    if stype not in TYPE_BPP:
        raise ValueError("unsupported sprite type %d" % stype)
    return TYPE_BPP[stype]


def pixels(data, off, wwords, hrows, fb, lb, bpp):
    """Return the rows of pixel values."""
    stride = (wwords + 1) * 4
    npix = ((wwords * 32) + lb - fb + 1) // bpp
    rows = []
    for y in range(hrows + 1):
        row = data[off + y * stride: off + (y + 1) * stride]
        bits = int.from_bytes(row, "little")
        vals = []
        mask = (1 << bpp) - 1
        for x in range(npix):
            vals.append((bits >> (fb + x * bpp)) & mask)
        rows.append(vals)
    return rows


def write_png(path, w, h, rgba_rows):
    raw = b"".join(b"\0" + bytes(r) for r in rgba_rows)

    def chunk(kind, body):
        c = kind + body
        return struct.pack(">I", len(body)) + c + \
            struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def convert(src, outdir):
    data = open(src, "rb").read()
    # a file lacks the area's first word (its size)
    count, first, _ = struct.unpack_from("<3I", data, 0)
    os.makedirs(outdir, exist_ok=True)
    o = first - 4
    for _ in range(count):
        nxt, = struct.unpack_from("<I", data, o)
        name = data[o + 4:o + 16].split(b"\0")[0].decode("latin-1")
        w, h, fb, lb, img, msk, mode = struct.unpack_from("<7I", data, o + 16)
        bpp = bpp_of(mode)
        if mode >= 256:
            fb = 0  # new-format sprites have no left-hand wastage

        ncols = 1 << bpp
        if img > 44:  # palette present: pairs of (first, second) flash words
            pal = []
            for i in range(min(ncols, (img - 44) // 8)):
                e, = struct.unpack_from("<I", data, o + 44 + i * 8)
                pal.append((e >> 8) & 0xFFFFFF)
            pal = [((c & 0xFF) << 16) | (c & 0xFF00) | (c >> 16) for c in pal]
        else:
            if bpp == 8:  # ponytail: no default 256-colour palette yet
                raise ValueError("%s: 8 bpp without a palette" % name)
            pal = {1: WIMP2, 2: WIMP4, 4: WIMP16}[bpp]

        rows = pixels(data, o + img, w, h, fb, lb, bpp)
        mrows = None
        if msk != img:
            if mode < 256:  # old format: mask has the image's layout
                mrows = pixels(data, o + msk, w, h, fb, lb, bpp)
            else:  # new format: 1 bpp mask, rows word-aligned
                last = len(rows[0]) - 1
                mrows = pixels(data, o + msk, last // 32, h, 0, last % 32, 1)

        out = []
        for y, row in enumerate(rows):
            line = []
            for x, v in enumerate(row):
                c = pal[v] if v < len(pal) else 0
                a = 255 if mrows is None or mrows[y][x] else 0
                line += [c >> 16 & 0xFF, c >> 8 & 0xFF, c & 0xFF, a]
            out.append(line)

        write_png(os.path.join(outdir, name + ".png"), len(rows[0]),
                  len(rows), out)
        print("%-12s %3dx%-3d %dbpp mode %s%s" %
              (name, len(rows[0]), len(rows), bpp, mode,
               " mask" if mrows else ""))
        o += nxt


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    convert(sys.argv[1], sys.argv[2])
