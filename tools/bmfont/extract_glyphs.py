#!/usr/bin/env python3
"""extract_glyphs.py -- find unique connected-colour glyph shapes in a
limited-colour PNG and write them out packed into a single grid image.

Background is auto-detected as the most frequent colour. Connected pixel
clusters (8-connected, exact colour match) are found per non-background
colour, small clusters are dropped as noise, and clusters are deduped by
their bounding-box pixel mask (position-independent).
"""

import argparse
import sys
from collections import Counter, deque

from PIL import Image


def find_clusters(pixels, width, height, background, min_pixels):
    visited = bytearray(width * height)
    clusters = []

    for start_y in range(height):
        for start_x in range(width):
            start_idx = start_y * width + start_x
            if visited[start_idx]:
                continue
            colour = pixels[start_idx]
            visited[start_idx] = 1
            if colour == background:
                continue

            coords = []
            queue = deque([(start_x, start_y)])
            while queue:
                x, y = queue.popleft()
                coords.append((x, y))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < width and 0 <= ny < height):
                            continue
                        nidx = ny * width + nx
                        if visited[nidx]:
                            continue
                        if pixels[nidx] != colour:
                            continue
                        visited[nidx] = 1
                        queue.append((nx, ny))

            if len(coords) >= min_pixels:
                clusters.append((colour, coords))

    return clusters


def normalize_mask(coords):
    coords = list(coords)
    min_x = min(x for x, _ in coords)
    min_y = min(y for _, y in coords)

    return frozenset((x - min_x, y - min_y) for x, y in coords)


def cluster_mask(coords):
    min_x = min(x for x, _ in coords)
    min_y = min(y for _, y in coords)
    max_x = max(x for x, _ in coords)
    max_y = max(y for _, y in coords)
    width = max_x - min_x + 1
    height = max_y - min_y + 1
    mask = normalize_mask(coords)

    return mask, width, height


def rotate_mask_90(mask):
    return normalize_mask((-y, x) for x, y in mask)


def mask_orientations(mask):
    orientations = [mask]
    for _ in range(3):
        orientations.append(rotate_mask_90(orientations[-1]))

    return orientations


def dedupe_clusters(clusters):
    seen = {}

    for colour, coords in clusters:
        mask, width, height = cluster_mask(coords)
        already_seen = any((colour, orientation) in seen
                            for orientation in mask_orientations(mask))
        if not already_seen:
            seen[(colour, mask)] = (colour, mask, width, height)

    return list(seen.values())


def pack_grid(glyphs, padding, background):
    cell_w = max(width for _, _, width, _ in glyphs) + padding * 2
    cell_h = max(height for _, _, _, height in glyphs) + padding * 2
    columns = max(1, int(len(glyphs) ** 0.5))
    rows = (len(glyphs) + columns - 1) // columns

    out = Image.new("RGB", (columns * cell_w, rows * cell_h), background)
    out_pixels = out.load()

    for index, (colour, mask, width, height) in enumerate(glyphs):
        col = index % columns
        row = index // columns
        origin_x = col * cell_w + padding
        origin_y = row * cell_h + padding
        for x, y in mask:
            out_pixels[origin_x + x, origin_y + y] = colour

    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="source PNG file")
    parser.add_argument("output", help="destination PNG file")
    parser.add_argument("--min-pixels", type=int, default=3,
                         help="drop clusters smaller than this (default: 3)")
    parser.add_argument("--cell-padding", type=int, default=2,
                         help="padding in pixels around each glyph in the "
                              "output grid (default: 2)")
    args = parser.parse_args()

    image = Image.open(args.input).convert("RGB")
    width, height = image.size
    pixels = list(image.getdata())

    background = Counter(pixels).most_common(1)[0][0]

    clusters = find_clusters(pixels, width, height, background,
                              args.min_pixels)
    if not clusters:
        print("No glyph clusters found.", file=sys.stderr)
        sys.exit(1)

    glyphs = dedupe_clusters(clusters)
    glyphs.sort(key=lambda glyph: len(glyph[1]))
    print(f"{len(clusters)} clusters, {len(glyphs)} unique forms "
          f"(background={background})")

    out = pack_grid(glyphs, args.cell_padding, background)
    out.save(args.output)


if __name__ == "__main__":
    main()
