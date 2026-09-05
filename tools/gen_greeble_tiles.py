#!/usr/bin/env python3
# gen_greeble_tiles.py -- bake the Xenon-2 greeble stamps into a C header
#
# Reads an 8xN paletted PNG of stacked 8x8 tiles (PICO-8 black / dark-purple /
# red / orange) and writes libraries/wuss/test/tasks/greeble-tiles.h:
#
#   greeble_tiles[NTILES][8]      one uint16 per row, 2 bits/pixel, x0 low
#   greeble_tile_edge[NTILES][4]  edge-colour-set class id, order N E S W
#
# Two tile edges may abut when their class ids are equal: the class is the set
# of colours appearing along that 8-pixel border strip, so "black frame meets
# black frame, red field meets red field" without demanding pixel alignment.
#
# Usage: tools/gen_greeble_tiles.py <tiles-only.png>

import sys
from PIL import Image

OUT = "libraries/wuss/test/tasks/greeble-tiles.h"
PAL = {(0, 0, 0, 255): 0, (126, 37, 83, 255): 1,
       (255, 0, 77, 255): 2, (255, 163, 0, 255): 3}


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: gen_greeble_tiles.py <tiles-only.png>")

    img = Image.open(sys.argv[1]).convert("RGBA")
    w, h = img.size
    if w != 8:
        sys.exit("expected an 8-pixel-wide sheet of stacked 8x8 tiles")
    n = h // 8

    def cell(i):
        px = img.crop((0, i * 8, 8, i * 8 + 8)).load()
        return [[PAL[px[x, y]] for x in range(8)] for y in range(8)]

    tiles = [cell(i) for i in range(n)]

    def strips(t):
        return (tuple(t[0]),                        # N
                tuple(t[y][7] for y in range(8)),   # E
                tuple(t[7]),                        # S
                tuple(t[y][0] for y in range(8)))   # W

    classes = sorted({frozenset(s) for t in tiles for s in strips(t)},
                     key=lambda fs: tuple(sorted(fs)))
    cid = {fs: i for i, fs in enumerate(classes)}

    def packrow(row):
        v = 0
        for x in range(8):
            v |= (row[x] & 3) << (2 * x)
        return v

    L = []
    L += ["/* greeble-tiles.h -- baked 8x8 greeble stamps + edge classes",
          " * generated from the Xenon 2 greebling tile sheet; do not hand-edit.",
          " * regenerate with tools/gen_greeble_tiles.py */",
          "",
          "#ifndef TASKS_GREEBLE_TILES_H",
          "#define TASKS_GREEBLE_TILES_H",
          "",
          "#define GREEBLE_NTILES %d" % n,
          "#define GREEBLE_NEDGECLASS %d" % len(classes),
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
    L += ["};",
          "",
          "/* edge-colour-set class per tile, order N, E, S, W; two edges may",
          " * abut when their class ids are equal */",
          "static const unsigned char greeble_tile_edge[GREEBLE_NTILES][4] =",
          "{"]
    for i, t in enumerate(tiles):
        nn, ee, ss, ww = strips(t)
        L.append("  { %2d, %2d, %2d, %2d }, /* %d */" % (
            cid[frozenset(nn)], cid[frozenset(ee)],
            cid[frozenset(ss)], cid[frozenset(ww)], i))
    L += ["};", "", "#endif /* TASKS_GREEBLE_TILES_H */", ""]

    with open(OUT, "w") as f:
        f.write("\n".join(L))
    print("wrote %s: %d tiles, %d edge classes" % (OUT, n, len(classes)))


if __name__ == "__main__":
    main()
