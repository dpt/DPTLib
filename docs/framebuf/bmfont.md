# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > bmfont

"bmfont" is a sub-library of DPTLib for drawing proportionally spaced bitmap fonts. It reads font definitions from PNG files like this:

![DPT-Henry Font](../resources/bmfonts/DPT-Henry/Regular.png)

or this:

![DPT-Digits Font](../resources/bmfonts/DPT-Digits/Regular.png)

or even this:

![Tiny Font](../resources/bmfonts/Tiny/Regular.png)

...which have the glyphs laid out in a grid, with extra lines inserted, that define the advance widths.

- It can draw to 1, 2, 4bpp, 16bpp (`rgb565`, `rgbx5551`) and 32bpp format screens at the time of writing.
- It supports both opaque and transparent backgrounds.
- Its rendering should be reasonably quick.

## PNG Font Format

The PNG should be 32 characters wide: bmfont works out the character dimensions from the total PNG size. It should be a paletted (colour type 3), 2 bits-per-pixel image with a four-entry palette:

- 0 -- background
- 1 -- glyph ink
- 2 -- advance-width marker (row 0 of each glyph cell: one pixel per column, present wherever that column's glyph should draw, sets the advance width)
- 3 -- grid/baseline (see below)

Pixel value 3 draws the cell's left sidebearing line (cosmetic only) plus two full-width rows that `bmfont_create` reads back to work out `ascent` and `descent`: one at the font's baseline, one at the cell bottom. It scans the first cell with no ink (normally the space glyph in grid column 0; see [Metrics](#metrics)) for the first two full-width rows of value-3 pixels within the cell body; the first is the baseline (its offset from the top of the cell is the ascent) and the second is the cell bottom (its offset from the baseline is the descent). A font predating this convention (no such rows found) falls back to treating the whole cell as ascent, with zero descent.

`tools/ttf2bmfont.py` generates conforming PNGs from a TTF automatically. `--no-grid` omits all the value-3 pixels, baseline row included -- don't pass it, or `bmfont_create` will fall back to the no-descender default for that font.

## Unicode Mapping

The PNG format above is unchanged. Unicode coverage is added by a TrueType-style character map (cmap) that maps codepoints to cells:

- **Glyph IDs.** Every cell in the PNG is a glyph, numbered in reading order: glyph ID = row × 32 + column. Blank trailing cells count, so a PNG with R rows has 32 × R glyphs and an ID never depends on content.
- **Unicode → glyph.** The cmap maps each codepoint to at most one glyph. Many codepoints may share a glyph (e.g. Latin A and Greek Alpha, or NBSP and space). Cells no codepoint reaches are ignored, so the PNG needn't be in any particular order.
- **Presence.** The cmap alone decides which codepoints exist. A mapped cell with no advance-width (value 2) pixels is a legitimate zero-advance glyph, such as a combining mark (placing it over its base glyph is a runtime concern).
- **Missing glyphs.** The glyph mapped from U+FFFD REPLACEMENT CHARACTER, if present, serves as .notdef. Otherwise the renderer draws a 1px hollow box as tall as the ascent, sitting on the baseline, and advances by the cell width. Unmapped codepoints below U+0020 are control characters: they draw nothing and advance nothing.

### The `.map` sidecar

The cmap lives in a plain-text sidecar next to the PNG, found by replacing the trailing `png` of the font's path with `map` (`Foo.png` → `Foo.map`, or `Foo/png` → `Foo/map` on RISC OS). Keeping it out of the PNG means the image can be round-tripped through a paint tool or Aseprite without losing it.

Each line is a sequential group, as in TrueType cmap format 12: a codepoint range and the glyph ID of its first codepoint, with subsequent codepoints taking consecutive glyphs. A single codepoint is a group of one.

```
# GrongyUI
U+0020..U+007E  0     # ASCII
U+00A0          0     # NBSP shares space
U+00A1..U+00FF  95
U+2190..U+2193  192   # arrows
```

- Codepoints are written `U+` followed by hex digits; the `U+` prefix is required so that bare-word lines remain free for future keywords. Glyph IDs are decimal.
- `#` starts a comment; blank lines are ignored.
- Groups must be in strictly ascending codepoint order and must not overlap.
- Every glyph a group reaches must be less than the glyph count (32 × rows).
- Codepoints must not exceed U+10FFFF.

Any violation makes `bmfont_create` fail with `result_BMFONT_BAD_MAP` rather than load a partially mapped font.

With no sidecar the font uses an implicit single group, `U+0020..` → glyph 0, ending at the last cell that has a non-empty advance-width strip. Trailing blank cells stay unmapped, so codepoints beyond the drawn glyphs still reach .notdef or a fallback font. All existing fonts therefore load as before.

### Metrics

The ascent and descent rows (see above) are read from the first cell that contains no ink (value 1), rather than always from grid column 0. For a Latin font that is still the space glyph; for a font whose first row is, say, arrows it is the first empty cell.

### Out of scope for the format

Coverage spanning several PNGs is a runtime concern: fonts are chained so that a glyph missing from one is taken from the next (e.g. GrongyUI falling back to Symbols). Wide (double-cell) glyphs belong in a separate font with a larger cell. Colour glyphs would need a different format.

## Setup

#### Make a bitmap and a screen:

1. `bitmap_init()` to create a `bitmap_t` for your destination buffer.
2. `screen_for_bitmap()` to create a `screen_t` from the `bitmap_t`.

#### Open a font:

`bmfont_create()` to load a font from a PNG, returning a font handle.

## Measuring

Use `bmfont_measure()` to measure and determine split points for runs of text:

```C
result_t bmfont_measure(bmfont_t       *bmfont,
                        const char     *text,
                        int             textlen,
                        bmfont_width_t  target_width,
                        int            *split_point,
                        bmfont_width_t *actual_width);
```

It requires a font handle, a pointer to some text, the number of bytes to consider and a target width. It returns a split point in bytes, and an actual width (both are optional - pass `NULL` if not required).

Units are in pixels.

Text is UTF-8 throughout. Lengths and indices count bytes, and any split point or caret index bmfont returns falls on a character boundary. Each malformed byte counts as U+FFFD.

## Drawing

Use `bmfont_draw()` to draw runs of text:

```C
result_t bmfont_draw(bmfont_t      *bmfont,
                     screen_t      *scr,
                     const char    *text,
                     int            len,
                     colour_t       fg,
                     colour_t       bg,
                     const point_t *pos,
                     point_t       *end_pos);
```

It requires a font handle, the screen to draw to, a pointer to some text, the number of bytes to consider, foreground and background colours, and a start position. It returns an end position (optional - pass `NULL` if not required).

The screen origin is at the top left. `pos` and `end_pos` are baseline positions, not the top-left of the glyph cells -- use `bmfont_get_info()`'s `ascent` out-param to convert from a top-left layout position (`baseline_y = top_y + ascent`).

```C
void bmfont_get_info(bmfont_t *bmfont,
                     int      *width,
                     int      *height,
                     int      *ascent,
                     int      *descent);
```

Returns the font's cell width, cell height, ascent and descent (any may be `NULL` if not required).

## Font families

Fonts live one level down, grouped by family: `resources/bmfonts/<Family>/<Style>.png`, with the `.map` sidecar beside it. Every font has a directory, even a family of one (`Tiny/Regular.png`). A sized cut of a design is its own family (`DPT-Digits` and `DPT-DigitsLg`), because size is not a style axis.

A style name is an optional weight (`Thin`, `Light`, `Regular`, `Medium`, `Bold` or `Black`) followed by an optional `Italic`, so `Bold.png`, `Italic.png` and `BoldItalic.png` are all valid. A name with no recognised weight counts as `Regular`.

`framebuf/bmfontfamily.h` reads a family's directory listing (no PNG is loaded) and answers style questions:

```c
bmfontfamily_t     *family;
const bmfontface_t *face;
bmfont_t           *font;

bmfontfamily_scan("resources/bmfonts/GrongyUI", &family);
face = bmfontfamily_find(family, bmfontfamily_WEIGHT_REGULAR,
                         bmfontfamily_SLANT_UPRIGHT);
face = bmfontfamily_heavier(family, face);   /* the Bold face */
bmfontcache_acquire(cache, face->path, &font);
bmfontfamily_destroy(family);
```

`bmfontfamily_heavier()` and `bmfontfamily_lighter()` step to the nearest face of the same slant and return the face they were given when there is nowhere further to go. `bmfontfamily_with_slant()` switches slant, keeping the weight or the nearest one.

`bmfont_enumerate()` walks every family under a directory and reports each face as a `Family Style` label (e.g. `GrongyUI Bold`), which `bmfontfamily_label_path()` turns back into a path.

## Limitations

- There's no font fallback chain yet, so a missing glyph draws .notdef rather than coming from another font.
- There's no tracking or kerning.
