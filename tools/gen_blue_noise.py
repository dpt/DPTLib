#!/usr/bin/env python3
"""gen_blue_noise.py -- emit the 64x64 blue-noise threshold map as C.

Ulichney's void-and-cluster method on a torus, so the tile repeats without
seams. Ranks 0..4095 are scaled down to 0..63 to match the 64-entry bias
table the dithered blit uses. Deterministic (fixed seed): rerun and paste the
output over blue_noise64x64 in libraries/framebuf/pattern/pattern.c.
"""

import math
import random

N     = 64
SIGMA = 1.5
SEED  = 1


def gaussian():
    g = [[0.0] * N for _ in range(N)]
    for dy in range(N):
        for dx in range(N):
            ddx = min(dx, N - dx)
            ddy = min(dy, N - dy)
            g[dy][dx] = math.exp(-(ddx * ddx + ddy * ddy) / (2 * SIGMA * SIGMA))
    return g


G = gaussian()


def splat(energy, x, y, sign):
    for yy in range(N):
        grow = G[(yy - y) % N]
        erow = energy[yy]
        for xx in range(N):
            erow[xx] += sign * grow[(xx - x) % N]


def energy_of(bits, value):
    energy = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            if bits[y][x] == value:
                splat(energy, x, y, 1)
    return energy


def extreme(bits, energy, value, pick):
    best = None
    where = None
    for y in range(N):
        for x in range(N):
            if bits[y][x] == value:
                e = energy[y][x]
                if best is None or pick(e, best):
                    best = e
                    where = (x, y)
    return where


def tightest_cluster(bits, energy, value=1):
    return extreme(bits, energy, value, lambda a, b: a > b)


def largest_void(bits, energy, value=0):
    return extreme(bits, energy, value, lambda a, b: a < b)


def main():
    rng = random.Random(SEED)

    # initial pattern: ~10% random minority pixels, relaxed into even spacing
    bits = [[0] * N for _ in range(N)]
    ones = N * N // 10
    for i in rng.sample(range(N * N), ones):
        bits[i // N][i % N] = 1

    energy = energy_of(bits, 1)
    while True:
        cx, cy = tightest_cluster(bits, energy)
        bits[cy][cx] = 0
        splat(energy, cx, cy, -1)
        vx, vy = largest_void(bits, energy)
        if (vx, vy) == (cx, cy):
            bits[cy][cx] = 1
            splat(energy, cx, cy, 1)
            break
        bits[vy][vx] = 1
        splat(energy, vx, vy, 1)

    proto = [row[:] for row in bits]
    proto_energy = [row[:] for row in energy]
    rank = [[0] * N for _ in range(N)]

    # phase 1: strip the prototype's points, tightest cluster first
    for r in range(ones - 1, -1, -1):
        x, y = tightest_cluster(bits, energy)
        bits[y][x] = 0
        splat(energy, x, y, -1)
        rank[y][x] = r

    # phase 2: fill the largest voids up to half full
    bits = proto
    energy = proto_energy
    for r in range(ones, N * N // 2):
        x, y = largest_void(bits, energy)
        bits[y][x] = 1
        splat(energy, x, y, 1)
        rank[y][x] = r

    # phase 3: zeros are now the minority; fill their tightest clusters
    energy = energy_of(bits, 0)
    for r in range(N * N // 2, N * N):
        x, y = tightest_cluster(bits, energy, 0)
        bits[y][x] = 1
        splat(energy, x, y, -1)
        rank[y][x] = r

    print("static const unsigned char blue_noise64x64[64][64] =")
    print("{")
    for y in range(N):
        vals = [rank[y][x] * 64 // (N * N) for x in range(N)]
        halves = [vals[i:i + 16] for i in range(0, N, 16)]
        lines = [", ".join("%2d" % v for v in h) for h in halves]
        print("  { " + ",\n    ".join(lines) + " }" + ("," if y < N - 1 else ""))
    print("};")


if __name__ == "__main__":
    main()
