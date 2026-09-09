# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > pixelmap

`pixelmap` caches lookup tables that convert between a "deep" (32bpp) pixel
format and a paletted (`p1`/`p2`/`p4`/`p8`) one, in either direction. It
replaces a per-pixel `colour_to_pixel` call in a blit inner loop with a table
read.

## Rationale

**Deep to paletted.** A bitmap-to-paletted blit inner loop otherwise calls
`colour_to_pixel` per pixel, and for a paletted target that runs a weighted
linear scan of the whole palette (`closest_palette_entry`) every time —
O(pixels x palette-entries) per blit. `pixelmap` does that scan once. The
source pixel's R, G and B channels are quantised to a few bits each, composed
into an index, and used to read a small table. Deep source colours are
deliberately quantised — a 16-colour palette cannot resolve finer than about
4:4:4 anyway — so the table stays resident: 2KB for a 32bpp-to-p4 map.

**Paletted to deep.** No palette scan is needed — the table is just
`nentries` deep pixels, indexed by the raw palette index. The win here is the
shared, build-once cache across repeated conversions of the same palette, and
one code path for both directions. `bmconv_p4_to_bgrx8888` was the
hand-written version of this (a local 16-entry `map[]`); it now calls
`pixelmap_get`.

## API

```C
const pixelmap_t *pixelmap_get(pixelfmt_t      srcfmt,
                               pixelfmt_t      destfmt,
                               const colour_t *palette,
                               int             nentries);
```

Exactly one of `srcfmt` / `destfmt` must be paletted and the other a 32bpp
`*8888` format; `palette` is that paletted side's palette. Returns a cached
table, or `NULL` if the pair is unsupported, `palette` is `NULL`, or
allocation failed. The pointer is owned by the module and stays valid until a
later `pixelmap_get` call with a different key evicts it.

```C
typedef struct pixelmap
{
  pixelfmt_t           srcfmt;
  pixelfmt_t           destfmt;
  int                  dest_log2bpp;    /* 0..3, deep->paletted only */
  unsigned char        entry_bytes;     /* 0 = packed paletted, else deep */
  unsigned char        rbits, gbits, bbits;
  unsigned char        rshift, gshift, bshift; /* into the source pixel */
  unsigned int         nentries;
  const unsigned char *entries;
}
pixelmap_t;
```

**Deep to paletted** (`entry_bytes == 0`). `entries` is packed to the
destination bit width, LSB-first within each byte: `p4` holds two 4-bit
entries per byte, `p2` four, `p1` eight, `p8` one byte per entry.

**Paletted to deep** (`entry_bytes == 4`). `entries` is `nentries` deep
pixels; the index is the raw palette index. `rbits`/`gbits`/`bbits` and the
shifts are 0.

## Using it — deep to paletted

A caller that blits in a loop fetches the table once, before the loop, and
bails if it is unsupported; then does a mask-and-lookup per pixel:

```C
const pixelmap_t *pm;

pm = pixelmap_get(pixelfmt_rgba8888, scr->format, scr->palette, 16);
if (pm == NULL)
  return result_NOT_SUPPORTED;
...
r   = (px >> pm->rshift) & 0xFF;
g   = (px >> pm->gshift) & 0xFF;
b   = (px >> pm->bshift) & 0xFF;
idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits))
    | ((g >> (8 - pm->gbits)) << pm->bbits)
    | ( b >> (8 - pm->bbits));
pxl = (pm->entries[idx >> 1] >> ((idx & 1) << 2)) & 0xF; /* p4 */
```

Alpha is not part of the index. Callers keep their own alpha test and route
only the opaque path through the table. There is no `colour_to_pixel`
fallback: a `NULL` return means the blit cannot proceed and the call sites
propagate `result_NOT_SUPPORTED`.

Call sites: `screen_copy_bitmap_p4` (`screen-draw.c`) and
`screen_copy_bitmap_rle_p4` (`screen-copy-bitmap-rle.c`). Both treat the source
as RGBA8888 byte order.

## Using it — paletted to deep

```C
const pixelmap_t          *pm;
const pixelfmt_bgrx8888_t *map;

pm  = pixelmap_get(pixelfmt_p4, pixelfmt_bgrx8888, src->palette, 16);
map = (const pixelfmt_bgrx8888_t *) pm->entries;
...
*outpx++ = map[nibble];
```

Call site: `bmconv_p4_to_bgrx8888` (`bitmap.c`).

## Policy

For a deep source, the channel split is chosen by a built-in table keyed on
`destfmt`. There is no caller override in v1.

| source           | dest        | bits/channel | entries | table bytes |
|------------------|-------------|--------------|---------|-------------|
| any 32bpp `*8888`| `p1`        | R4 G4 B4     | 4096    | 512         |
| any 32bpp `*8888`| `p2`        | R4 G4 B4     | 4096    | 1024        |
| any 32bpp `*8888`| `p4`        | R4 G4 B4     | 4096    | 2048        |
| any 32bpp `*8888`| `p8`        | R5 G6 B5     | 65536   | 65536       |
| `p1`..`p8`       | 32bpp `*8888` | *(raw index)* | palette size | 4 x entries |

`p8` keeps a wider index so 256-colour output stays faithful. A paletted
source has no quantisation — one deep pixel per palette entry.

## Cache

A module-global LRU of 4 entries, keyed on `(srcfmt, destfmt, FNV-1a hash of
the palette bytes)`. Hashing the palette contents (rather than its address)
keeps the key correct when a palette is mutated in place or a freed palette's
address is reused. The design mirrors `spanregistry_get`. Single-threaded, like
the rest of `framebuf`.

## Behaviour change on adoption

Converting a **deep-to-paletted** call site from `colour_to_pixel` to
`pixelmap` changes its output slightly: the old path matched on the exact
24-bit source colour, the new path quantises to the policy split first. For a
<=16-entry palette this is imperceptible; a palette with two entries inside
one quantisation cell could move a pixel by one index. The converted sites
(`screen_copy_bitmap_p4`, `screen_copy_bitmap_rle_p4`) now return
`result_NOT_SUPPORTED` for a format pair `pixelmap` cannot map, where before
they would have limped along one `colour_to_pixel` call per pixel.

A **paletted-to-deep** call site is bit-exact — the entries are the same
`colour_to_pixel` results the local `map[]` held.

## Build cost

For a deep source, building a table is one `colour_to_pixel` call per index
value, at `get` time only: ~65K calls for a 32bpp-to-p4 map (negligible),
~16M for a `p8` map (a few milliseconds). For a paletted source it is one
call per palette entry (16 or 256). Amortised across every subsequent blit
that hits the cache.

## Not done

- No 16bpp (`565`) support on either side — add when a call site needs it.
- Paletted-to-deep only fills 4-byte (`*8888`) entries; 12/15/16bpp deep
  destinations are not built.
- No `pixelmap_get_ex` with a caller-supplied channel split.
- `span_p4_blendconst` still re-quantises per pixel; its input is a blend
  result, not a source pixel, so it is a separate change.

## Related

- [drawing.md](drawing.md) — the `framebuf` drawing taxonomy
- [rle-bitmap.md](rle-bitmap.md) — RLE bitmaps; its p4 blit path uses pixelmap
