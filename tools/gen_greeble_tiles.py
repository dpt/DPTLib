#!/usr/bin/env python3
# gen_greeble_tiles.py -- bake the Xenon-2 greeble stamps into a C header
#
# Reads an 8xN paletted PNG of stacked 8x8 tiles (PICO-8 black / dark-purple /
# red / orange) and writes libraries/wuss/test/tasks/greeble-tiles.h:
#
#   greeble_tiles[NTILES][8]      one uint16 per row, 2 bits/pixel, x0 low
#
# With a second image -- a sheet where the artist has laid the stamps out in
# their intended shapes -- the connected non-empty clusters become prefab
# blocks in the same header:
#
#   greeble_prefab_cells[]        flat pool of stamp indices, 0xFF = hole
#   greeble_prefab[NPREFAB]       { w, h, offset into the pool }
#   greeble_filler[NFILLER]       stamp indices the artist left standing alone
#
# 1x1 clusters are not prefabs; they are the artist's shortlist of stamps that
# read well dropped in on their own, so they become greeble_filler[] -- what
# the generator scatters into whatever space the blocks leave.
#
# Usage: tools/gen_greeble_tiles.py <tiles-only.png> [tiles-arranged.png]

import sys
from PIL import Image

OUT = "libraries/wuss/test/tasks/greeble-tiles.h"
PAL = {(0, 0, 0, 255): 0, (126, 37, 83, 255): 1,
       (255, 0, 77, 255): 2, (255, 163, 0, 255): 3}
HOLE = 0xFF


def load_tiles(path):
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    if w != 8:
        sys.exit("expected an 8-pixel-wide sheet of stacked 8x8 tiles")
    n = h // 8
    px = img.load()

    def cell(i):
        return [[PAL[px[x, i * 8 + y]] for x in range(8)] for y in range(8)]

    return [cell(i) for i in range(n)]


def packrow(row):
    v = 0
    for x in range(8):
        v |= (row[x] & 3) << (2 * x)
    return v


def tile_key(t):
    return tuple(v for r in t for v in r)


def find_prefabs(path, tiles):
    """Slice the arrangement sheet into 8x8 cells, resolve each non-empty
    cell to a stamp index, group into 4-connected clusters. Returns
    (prefabs, filler): prefabs are the multi-tile clusters as
    (w, h, [row-major indices]) with HOLE for uncovered cells; filler is the
    list of stamp indices that stood alone (1x1 clusters), de-duplicated in
    first-seen order."""
    lut = {tile_key(t): i for i, t in enumerate(tiles)}
    img = Image.open(path).convert("RGBA")
    W, H = img.size
    gw, gh = W // 8, H // 8
    px = img.load()

    def resolve(cx, cy):
        rgba = [px[cx * 8 + x, cy * 8 + y] for y in range(8) for x in range(8)]
        if any(p[3] == 0 for p in rgba):
            return None
        try:
            key = tuple(PAL[p] for p in rgba)
        except KeyError:
            sys.exit("arrangement cell (%d,%d) uses a non-palette colour"
                     % (cx, cy))
        if key not in lut:
            sys.exit("arrangement cell (%d,%d) matches no tile in the sheet"
                     % (cx, cy))
        return lut[key]

    idx = [[resolve(x, y) for x in range(gw)] for y in range(gh)]
    seen = [[False] * gw for _ in range(gh)]
    prefabs = []
    filler = []
    for y in range(gh):
        for x in range(gw):
            if idx[y][x] is None or seen[y][x]:
                continue
            stack = [(x, y)]
            cells = []
            seen[y][x] = True
            while stack:
                a, b = stack.pop()
                cells.append((a, b))
                for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    na, nb = a + da, b + db
                    if 0 <= na < gw and 0 <= nb < gh \
                       and idx[nb][na] is not None and not seen[nb][na]:
                        seen[nb][na] = True
                        stack.append((na, nb))
            if len(cells) < 2:
                v = idx[y][x]
                if v not in filler:
                    filler.append(v)
                continue
            xs = [a for a, _ in cells]
            ys = [b for _, b in cells]
            x0, y0 = min(xs), min(ys)
            w, h = max(xs) - x0 + 1, max(ys) - y0 + 1
            flat = [HOLE] * (w * h)
            for a, b in cells:
                flat[(b - y0) * w + (a - x0)] = idx[b][a]
            prefabs.append((w, h, flat))
    return prefabs, filler


def main():
    if not 2 <= len(sys.argv) <= 3:
        sys.exit("usage: gen_greeble_tiles.py <tiles-only.png> "
                 "[tiles-arranged.png]")

    tiles = load_tiles(sys.argv[1])
    n = len(tiles)

    if len(sys.argv) == 3:
        prefabs, filler = find_prefabs(sys.argv[2], tiles)
    else:
        prefabs, filler = [], list(range(n))

    L = []
    L += ["/* greeble-tiles.h -- baked 8x8 greeble stamps",
          " * generated from the Xenon 2 greebling tile sheet; do not hand-edit.",
          " * regenerate with tools/gen_greeble_tiles.py */",
          "",
          "#ifndef TASKS_GREEBLE_TILES_H",
          "#define TASKS_GREEBLE_TILES_H",
          "",
          "#define GREEBLE_NTILES %d" % n,
          "#define GREEBLE_TILE_PX 8",
          "",
          "/* each tile: 8 rows, one uint16 per row, 2 bits per pixel (x0 in the",
          " * low bits); pixel value 0..3 indexes greeble_palette[] in greeble.c */",
          "static const unsigned short greeble_tiles[GREEBLE_NTILES]"
          "[GREEBLE_TILE_PX] =",
          "{"]
    for i, t in enumerate(tiles):
        row = ", ".join("0x%04X" % packrow(t[y]) for y in range(8))
        L.append("  { %s }, /* %d */" % (row, i))
    L += ["};", ""]

    pool = []
    spans = []
    for w, h, flat in prefabs:
        spans.append((w, h, len(pool)))
        pool.extend(flat)

    L += ["/* prefab blocks lifted from the arrangement sheet: each is a w*h",
          " * grid of stamp indices in greeble_prefab_cells[], 0x%02X marking a"
          % HOLE,
          " * cell the block's bounding box does not fill. 1x1 clusters are",
          " * dropped. GREEBLE_NPREFAB is 0 when the sheet was not supplied. */",
          "#define GREEBLE_NPREFAB %d" % len(spans),
          "#define GREEBLE_PREFAB_HOLE 0x%02X" % HOLE,
          ""]
    if spans:
        L += ["static const unsigned char greeble_prefab_cells[] =",
              "{"]
        for w, h, off in spans:
            for r in range(h):
                seg = pool[off + r * w: off + r * w + w]
                L.append("  %s," % ", ".join("0x%02X" % v for v in seg))
        L += ["};",
              "",
              "static const struct { unsigned char w, h; unsigned short off; }",
              "greeble_prefab[GREEBLE_NPREFAB] =",
              "{"]
        for i, (w, h, off) in enumerate(spans):
            L.append("  { %2d, %2d, %4d }, /* %d */" % (w, h, off, i))
        L += ["};", ""]

    L += ["/* stamps the generator scatters into cells no prefab claimed: the",
          " * 1x1 clusters from the arrangement sheet, or all tiles when no",
          " * sheet was supplied */",
          "#define GREEBLE_NFILLER %d" % len(filler),
          "static const unsigned char greeble_filler[GREEBLE_NFILLER] =",
          "{"]
    for i in range(0, len(filler), 12):
        L.append("  %s," % ", ".join("%3d" % v for v in filler[i:i + 12]))
    L += ["};", ""]

    L += ["#endif /* TASKS_GREEBLE_TILES_H */", ""]

    with open(OUT, "w") as f:
        f.write("\n".join(L))
    print("wrote %s: %d tiles, %d prefabs, %d filler"
          % (OUT, n, len(spans), len(filler)))


if __name__ == "__main__":
    main()
