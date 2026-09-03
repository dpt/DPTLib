# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > drawing

This document defines the **taxonomy and naming scheme** for 2D drawing
operations in the `framebuf` module: primitives, fills, blits, and the paints
that feed them.

It is a design agreement. Some of the names below are already shipped; others
are the agreed target for code not yet written. Each function's status is marked
**[shipped]** or **[planned]**.

## Rationale

`framebuf` grew its drawing primitives one at a time (`screen_draw_line`,
`screen_fill_rect`, `screen_fill_pattern`, `screen_draw_ninepatch`,
`bitmap_draw_pattern`, ...). Without a scheme the names had begun to contradict
each other:

- "pattern" meant two different things (a two-colour repeating tile vs a
  one-colour stencil mask), spelled with two different verbs (`fill` vs `draw`);
- `draw` was overloaded across stroking an outline, blitting a raster, and
  placing text;
- "ninepatch" (code) / "9-patch" (prose) / "9tile" (art asset) were three
  spellings of one thing.

New primitives are wanted (filled triangles and polygons, rounded rectangles,
gradients, sprite-sheet tiling). A scheme fixed now keeps their names
predictable.

## The four axes

Every drawing operation is one point in a four-axis space. The function name is
built from the axes in order:

```
<surface>_<action>_<primitive|raster>[_<paint>][_<qualifier>]
```

### Axis 1 — SURFACE (where it draws)

| prefix   | surface | clipping |
|----------|---------|----------|
| `screen` | `screen_t` — the drawing surface | clips every op to `scr->clip` |
| `bitmap` | `bitmap_t` — a raw image | none; whole-image or caller-clipped |

`screen_t` is `bitmap_ALL_MEMBERS` plus a `box_t clip`. It is the surface a
client or the window manager draws onto. `bitmap_*` drawing ops are for
off-screen construction and whole-image transforms.

The per-pixel-format `span_t` layer (`copy` / `blendconst` / `blendarray`) sits
below both and is not a public drawing verb.

### Axis 2 — ACTION (what kind of mark)

| verb    | mark | object it takes |
|---------|------|-----------------|
| `draw`  | a 1D mark along a boundary: an outline, a line, a polyline | a **primitive** |
| `fill`  | a 2D mark over an interior | a **primitive** |
| `copy`  | pixels transferred from another raster | a **raster source** |
| `clear` | an unconditional whole-surface fill | — |

`draw` and `fill` are the only verbs that name a geometric primitive. `copy` is
the single verb for all pixel transfer — a bitmap, a ninepatch, a sprite-sheet
cell, or a screen-to-screen move. `clear` is `bitmap_clear` only.

Text (`bmfont_draw`, `bmtext_draw`) keeps its own verb: a string is neither a
primitive nor a raster.

### Axis 3 — PRIMITIVE (the shape), for `draw` / `fill`

Ordered by rough complexity:

`pixel`, `line`, `lines` (polyline), `rect`, `square`, `box`, `roundrect`,
`circle`, `ellipse`, `arc`, `triangle`, `polygon`, `region` (a list of boxes).

`rect` takes `x, y, size2d_t` with **inclusive** edges (historical).
`box` takes a `const box_t *` — the **half-open** rectangle used everywhere else
in DPTLib. New code should prefer `box`.

### Axis 4 — PAINT (what goes inside), a suffix

How a `fill` (or `clear`, or a wide `draw` stroke) obtains its pixels:

| paint    | suffix      | source |
|----------|-------------|--------|
| colour   | *(none)*    | a single `colour_t` |
| pattern  | `_pattern`  | an 8x8 repeating tile — a `pattern_t` |
| gradient | `_gradient` | an interpolated ramp between stops |

Sampling from a raster is **not** a paint suffix — that is `copy`.

## Examples

```
screen_fill_hline             colour fill, one horizontal run       [shipped]
screen_fill_rect              colour fill, inclusive-edge rect       [shipped]
screen_fill_box               colour fill, half-open box            [planned]
screen_draw_circle            1px circle outline                     [shipped]
screen_fill_circle            solid disc                             [shipped]
screen_fill_pattern           pattern fill on the screen            [shipped]
bitmap_fill_pattern           pattern fill on a raw bitmap          [shipped; was bitmap_draw_pattern]
screen_fill_rect_gradient     gradient fill of a rect              [planned]
screen_copy_bitmap            blit a bitmap                        [shipped; was screen_draw_bitmap]
screen_copy_ninepatch         blit a resizable 9-patch frame       [shipped; was screen_draw_ninepatch]
screen_copy_rect              screen-to-screen block move           [shipped]
screen_draw_line_wu_float     anti-aliased line, float coords       [shipped]
```

## Patterns

A **pattern** is one concept with one struct and one verb (`fill`):

```C
typedef struct pattern
{
  uint8_t   bits[8];   /* one byte per row, MSB = leftmost pixel      */
  colour_t  fg;         /* painted where a bit is set                 */
  colour_t  bg;         /* painted where a bit is clear (see flags)   */
  unsigned  flags;      /* pattern_FLAG_STENCIL: leave bg pixels as-is */
  point_t   origin;     /* tile phase; {0,0} locks to surface origin  */
}
pattern_t;
```

- A plain two-colour tile sets `fg`, `bg` and no flags — every pixel in the area
  is written.
- A stencil sets `pattern_FLAG_STENCIL` — only set-bit pixels are written, the
  rest of the area is untouched. (This is the old `bitmap_draw_pattern`
  behaviour.)
- `origin` phases the tile so abutting fills line up regardless of the fill
  rectangle. `{0,0}` phases against the surface origin.

The built-in tiles — the 65-level Bayer coverage ramp and the named tiles
(`HSTRIPE`, `GRID`, `CROSSHATCH`, ...) — remain available as a
`screen_pattern_t` enum, resolved to a `pattern_t` by a
`pattern_from_preset(screen_pattern_t)` helper.

Format support: 8bpp paletted and 32bpp. Other depths return
`result_NOT_SUPPORTED`.

## Ninepatch

The one-word lowercase spelling **`ninepatch`** is canonical in all code and
identifiers. "9-patch" is fine in prose. The art asset is `ninepatch.png` /
`ninepatch.aseprite`.

`screen_copy_ninepatch(scr, dst, src, flags)` draws a resizable frame from a
3x3-cell source image (width and height each a positive multiple of 3): the four
corners are drawn at natural size, the four edges are tiled along their run, and
the centre is tiled to fill (unless `screen_NINEPATCH_NO_CENTRE` is set). The
clip is restored on return.

## Error reporting

New drawing ops return **`result_t`**. An op that clips away to nothing is not an
error — it returns `result_OK` and draws nothing.

Existing `void`-returning `screen_draw_*` ops keep their return type. The three
`copy` ops are aligned on `result_t`: `screen_copy_bitmap`,
`screen_copy_ninepatch` and `screen_copy_rect` all return `result_OK` on
success and `result_NOT_SUPPORTED` when the screen's pixel format has no blit
path (`screen_copy_rect` also returns `result_NOT_SUPPORTED` when clipping
leaves nothing to copy, since the caller must then fall back to a full
redraw). `screen_copy_rect` keeps its `box_t *copied_dst` out-param.

## Rectangle representation

`box_t` (`{x0, y0, x1, y1}`, half-open — `x1`, `y1` exclusive) is the one
geometric rectangle type. New primitives take `const box_t *`. There is no
separate `rect_t`. `screen_fill_rect`'s inclusive `x, y, size2d_t` signature is
retained; `screen_fill_box` / `screen_draw_box` are the preferred `box_t`
spellings going forward.

## Status summary

**Shipped**

- `screen_set_pixel`
- `screen_draw_line`, `screen_draw_lines`, `screen_draw_dashed_line`,
  `screen_draw_line_wu_fix8`, `screen_draw_line_wu_float`
- `screen_draw_rect`, `screen_fill_rect`, `screen_fill_square`,
  `screen_fill_hline` (the per-row primitive `screen_fill_rect` and
  `screen_draw_circle` build on)
- `screen_draw_circle`, `screen_fill_circle`
- `screen_fill_pattern` (takes `const pattern_t *`)
- `screen_copy_bitmap`, `screen_copy_ninepatch`, `screen_copy_rect` — all
  return `result_t`
- `bitmap_clear`, `bitmap_fill_pattern` (`const pattern_t *`, returns
  `result_t`)
- `bmfont_draw`, `bmtext_draw`
- `composite` — see [composite.md](composite.md)

The rename pass was hard renames, no compatibility wrappers, all in-tree call
sites updated in the same change: `screen_draw_bitmap` → `screen_copy_bitmap`,
`screen_draw_ninepatch` → `screen_copy_ninepatch`, `bitmap_draw_pattern` →
`bitmap_fill_pattern` (now taking a `pattern_t`).

**Planned — new primitives** (priority order)

1. `screen_fill_triangle` / `screen_draw_triangle`
2. `screen_fill_polygon` / `screen_draw_polygon` (convex first)
3. `screen_fill_roundrect` / `screen_draw_roundrect`
4. `screen_fill_rect_gradient` (linear, two-stop, Bayer-dithered)
5. `screen_copy_cell` (single sprite-sheet cell)
6. `screen_draw_arc` / `screen_draw_ellipse`

## Related

- [composite.md](composite.md) — Porter-Duff bitmap compositing
- [bmfont.md](bmfont.md) — proportional bitmap fonts
