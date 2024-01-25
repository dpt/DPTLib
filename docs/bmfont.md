[DPTLib](https://github.com/dpt/DPTLib) > framebuf > bmfont
===========================================================
"bmfont" is a sub-library of DPTLib for drawing proportionally spaced bitmap fonts. It reads font definitions from PNG files like this:

![Henry Font](../resources/bmfonts/henry-font.png)

or this:

![Digits Font](../resources/bmfonts/digits-font.png)

or even this:

![Tiny Font](../resources/bmfonts/tiny-font.png)

...which have the glyphs laid out in a grid, with extra lines inserted, that define the advance widths.

- It can draw to 4bpp and 32bpp format screens at the time of writing.
- It supports both opaque and transparent backgrounds.
- Its rendering should be reasonably quick.

PNG Font Format
---------------
The PNG should be 32 characters wide: bmfont works out the character dimensions from the total PNG size.

It should be a four colour PNG where pixels of value 1 are font definitions and pixels of value 2 are advance widths. Note: An 8bpp 4-palette-entry image won't do, it must be 4bpp.

Setup
-----
#### Make a bitmap and a screen:
1. `bitmap_init()` to create a `bitmap_t` for your destination buffer.
2. `screen_for_bitmap()` to create a `screen_t` from the `bitmap_t`.

#### Open a font:
`bmfont_create()` to load a font from a PNG, returning a font handle.

Measuring
---------
Use `bmfont_measure()` to measure and determine split points for runs of text:

``` C
result_t bmfont_measure(bmfont_t       *bmfont,
                        const char     *text,
                        int             textlen,
                        bmfont_width_t  target_width,
                        int            *split_point,
                        bmfont_width_t *actual_width);
```

It requires a font handle, a pointer to some text, the number of characters to consider and a target width. It returns a split point, and an actual width (both are optional - pass `NULL` if not required).
 
Units are in pixels.

Drawing
-------
Use `bmfont_draw()` to draw runs of text:

``` C
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

The screen origin is at the top left.

Limitations
-----------
* There's no character mapping yet - characters are treated as bytes, not UTF-8.
* There's no tracking or kerning.
