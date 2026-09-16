# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > bmfont

"bmfont" is a sub-library of DPTLib for drawing proportionally spaced bitmap fonts. It reads font definitions from PNG files like this:

![DPT-Henry Font](../resources/bmfonts/DPT-Henry.png)

or this:

![DPT-Digits Font](../resources/bmfonts/DPT-Digits-Regular.png)

or even this:

![Tiny Font](../resources/bmfonts/tiny.png)

...which have the glyphs laid out in a grid, with extra lines inserted, that define the advance widths.

- It can draw to 4bpp and 32bpp format screens at the time of writing.
- It supports both opaque and transparent backgrounds.
- Its rendering should be reasonably quick.

## PNG Font Format

The PNG should be 32 characters wide: bmfont works out the character dimensions from the total PNG size. It should be a paletted (colour type 3), 2 bits-per-pixel image with a four-entry palette:

- 0 -- background
- 1 -- glyph ink
- 2 -- advance-width marker (row 0 of each glyph cell: one pixel per column, present wherever that column's glyph should draw, sets the advance width)
- 3 -- grid/baseline (see below)

Pixel value 3 draws the cell's left sidebearing line (cosmetic only) plus two full-width rows that `bmfont_create` reads back to work out `ascent` and `descent`: one at the font's baseline, one at the cell bottom. It scans the space glyph's cell (grid column 0, which carries no ink to interfere) for the first two full-width rows of value-3 pixels within the cell body; the first is the baseline (its offset from the top of the cell is the ascent) and the second is the cell bottom (its offset from the baseline is the descent). A font predating this convention (no such rows found) falls back to treating the whole cell as ascent, with zero descent.

`tools/ttf2bmfont.py` generates conforming PNGs from a TTF automatically. `--no-grid` omits all the value-3 pixels, baseline row included -- don't pass it, or `bmfont_create` will fall back to the no-descender default for that font.

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

It requires a font handle, a pointer to some text, the number of characters to consider and a target width. It returns a split point, and an actual width (both are optional - pass `NULL` if not required).

Units are in pixels.

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

It requires a font handle, the screen to draw to, a pointer to some text, the number of characters to consider, foreground and background colours, and a start position. It returns an end position (optional - pass `NULL` if not required).

The screen origin is at the top left. `pos` and `end_pos` are baseline positions, not the top-left of the glyph cells -- use `bmfont_get_info()`'s `ascent` out-param to convert from a top-left layout position (`baseline_y = top_y + ascent`).

```C
void bmfont_get_info(bmfont_t *bmfont,
                     int      *width,
                     int      *height,
                     int      *ascent,
                     int      *descent);
```

Returns the font's cell width, cell height, ascent and descent (any may be `NULL` if not required).

## Limitations

- There's no character mapping yet - characters are treated as bytes, not UTF-8.
- There's no tracking or kerning.
