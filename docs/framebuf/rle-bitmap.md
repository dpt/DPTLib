# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > RLE-compressed bitmaps

An optional **read-only run-length-encoded** representation of a `bitmap_t`.
UI art, icons and nine-patches are typically sparse: long runs of one colour
and large fully-transparent regions. Storing them raw wastes RAM and, at blit
time, memory bandwidth. `bitmap_compress()` squashes a loaded bitmap in place
into a per-scanline ("laterally counted") opcode stream; `screen_copy_bitmap()`
decodes it on the fly, skipping transparent runs by pointer arithmetic.

## Lifecycle

```C
bitmap_t bm;
bitmap_load_png(&bm, "icon.png");   /* raw rgba8888 */
bitmap_compress(&bm);               /* now RLE, bm.base is a blob */

screen_copy_bitmap(&scr, x, y, &bm);/* blits straight from the blob */

/* bitmap_decompress(&bm); to get raw pixels back, or just: */
free(bm.base);
```

`bitmap_compress()` is idempotent. `bitmap_decompress()` restores the raw
`rowbytes`-strided buffer byte-for-byte and is a no-op on an uncompressed
bitmap.

## The compressed flag

There is no new `bitmap_t` field. Bit 30 of `bm->format` (`pixelfmt_RLE_FLAG`,
clear of the sign bit) marks a compressed pixel store. The low bits still hold
the real `pixelfmt_t`, so `bm->size`, `bm->palette` and `bm->span` stay valid
while compressed; `bm->rowbytes` is 0.

- `pixelfmt_is_rle(f)` / `bitmap_is_compressed(bm)` — test the flag.
- `pixelfmt_base(f)` — strip the flag, yielding the decoded pixel format.

`pixelfmt_log2bpp()` and `spanregistry_get()` are **not** flag-tolerant: a
compressed format reaching them is a missing-guard bug and trips `assert(0)`.
Every other bitmap entry point (`bitmap_clear`, `bitmap_fill_pattern`,
`bitmap_convert`, `bitmap_save_png`, `composite`, `screen_copy_ninepatch`)
guards with `pixelfmt_is_rle()` and declines with `result_NOT_SUPPORTED`.

## Blob layout

`bm->base` points at an 8-byte header followed by `bm->size.h` opcode streams,
one per row, concatenated, each EOL-terminated. No magic, no format/size (all
still in the struct), no row-offset index.

| bytes | field      | meaning                                              |
|-------|------------|-----------------------------------------------------|
| 0..3  | `rowbytes` | u32 LE — original stride, restored by decompress    |
| 4..7  | `data_len` | u32 LE — encoded byte count that follows            |

To blit from a clipped top row the decoder walks the opcode stream forward
(no writes) counting EOL bytes — `bitmap__rle_skip_row()`.

## Row opcodes

Dispatch is by the count of leading zero bits of the first byte (`clz8`,
UTF-8-prefix style). `PIXEL` is 1 byte for an 8bpp base, 4 bytes for 32bpp.
Long forms store `length - shortmax - 1`; a zero length field means `2^bits`.

```
1LLLLLLL                     literal      : 1..128 pixels follow inline
01LLLLLL PIXEL               repeat       : 1..64 copies of one inline pixel
001LLLLL                     skip         : 1..32 zero pixels        [alpha only]
0001LLLL LLLLLLLL            literal long : 129..4224 pixels follow inline
00001LLL LLLLLLLL PIXEL      repeat long  : 65..2112 copies
000001LL LLLLLLLL            skip long    : 33..1056 zero pixels     [alpha only]
00000001                     EOL
00000000                     reserved (decoder rejects)
```

A `skip` run drops the pixel bytes entirely, so it is only emitted for a
**wholly zero** pixel (`0x00000000`, the usual PNG transparent value).
Non-zero-but-alpha-0 pixels are encoded as literal/repeat and survive a round
trip. `bitmap_decompress()` writes zeros for skip runs; the blit leaves the
destination untouched.

## Supported formats

| base format                                   | compress | skip runs |
|-----------------------------------------------|----------|-----------|
| `pixelfmt_y8`                                  | yes      | no        |
| `pixelfmt_{bgrx,rgbx,xbgr,xrgb}8888`           | yes      | no        |
| `pixelfmt_{bgra,rgba}8888`                     | yes      | yes       |
| `pixelfmt_{abgr,argb}8888`                     | yes      | no        |
| `p1`..`p8`, 12/15/16bpp                        | no (`result_NOT_SUPPORTED`) | — |

## Screen targets

- **32bpp screen** — the source must decode to the screen's exact channel
  order; the blit is then a byte copy, no per-pixel conversion
  (`pixelfmt_base(src->format) == scr->format`, alpha folded out).
- **p4 screen** — each decoded pixel is mapped to the nearest palette index. A
  cached [pixelmap](pixelmap.md) table (`pixelmap_get(pixelfmt_rgba8888,
  scr->format, scr->palette, 16)`) turns the inner loop into a mask-and-lookup;
  if that pair is unsupported the blit declines `result_NOT_SUPPORTED`.
- **Other depths** decline `result_NOT_SUPPORTED`.

## v1 limitations

- **Alpha-tested, not alpha-blended.** Transparent runs are skipped, opaque
  runs copied. This matches `screen_copy_bitmap`'s p4 path but not its 32bpp
  path (which blends per pixel). A blended RLE path would decode alpha runs
  into `span->blendarray`.
- No scanline dedup, no row-offset table — pure sequential decode.
- No `composite()` or nine-patch support for compressed bitmaps.
- No colour-key transparency — wholly-zero pixels only.
- Runtime `bitmap_compress()` only — no `.rle` file format or offline tool.
