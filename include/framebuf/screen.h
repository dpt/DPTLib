/* framebuf/screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "geom/box.h"
#include "geom/point.h"
#include "utils/fxp.h"

// TODO: Define screen origin etc.

/**
 * A screen.
 */
typedef struct screen
{
  bitmap_ALL_MEMBERS
  box_t clip; /* rectangular clip region, specified in pixels */
}
screen_t;

/**
 * Initialize a previously allocated screen structure.
 *
 * \param[in] scr       Screen to initialize.
 * \param[in] size      Width and height of the screen in pixels.
 * \param[in] fmt       Pixel format of the screen.
 * \param[in] rowbytes  Number of bytes per row of the screen.
 * \param[in] palette   Palette of the screen, or NULL.
 * \param[in] base      Base address of the screen.
 */
void screen_init(screen_t  *scr,
                 size2d_t   size,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 void      *base);

/**
 * Initialize a previously allocated screen structure, for drawing to an
 * existing bitmap.
 *
 * \param[in] scr Screen to initialize.
 * \param[in] bm  Bitmap to draw to.
 */
void screen_for_bitmap(screen_t *scr, const bitmap_t *bm);

/**
 * Read the clipping box of a screen.
 *
 * \param[in]  scr  Screen to read clipping box of.
 * \param[out] clip Clipping box.
 * \return True if clipping box is valid, false otherwise.
 */
int screen_get_clip(const screen_t *scr, box_t *clip);

/**
 * Draws a single pixel.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of pixel to draw.
 * \param[in] y       Y coordinate of pixel to draw.
 * \param[in] colour  Colour of pixel.
 */
void screen_set_pixel(screen_t *scr, int x, int y, colour_t colour);

/**
 * Draws a solid rectangle.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of leftmost point of rectangle.
 * \param[in] y       Y coordinate of topmost point of rectangle.
 * \param[in] size    Width and height of rectangle.
 * \param[in] colour  Colour of rectangle.
 */
void screen_fill_rect(screen_t *scr,
                      int       x,
                      int       y,
                      size2d_t  size,
                      colour_t  colour);

/**
 * Special case of `screen_fill_rect`.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of leftmost point of rectangle.
 * \param[in] y       Y coordinate of topmost point of rectangle.
 * \param[in] size    Size of rectangle.
 * \param[in] colour  Colour of rectangle.
 */
void screen_fill_square(screen_t *scr,
                        int       x,
                        int       y,
                        int       size,
                        colour_t  colour);

/**
 * Built-in 8x8 fill patterns for `screen_fill_pattern`. Each is a 1-bit
 * tile: set bits take the foreground colour, clear bits the background.
 */
typedef enum screen_pattern
{
  /**
   * 8x8 ordered (Bayer) dither, one entry per coverage level 0 (empty) to 64
   * (solid). Index by level as `screen_PATTERN_BAYER0 + level`;
   * `screen_PATTERN_BAYER_LIMIT` is one past the last.
   */
  screen_PATTERN_BAYER0 = 0,
  screen_PATTERN_BAYER_LIMIT = screen_PATTERN_BAYER0 + 65,

  /**
   * A flat fill and a 50% checkerboard are just the ends and midpoint of the
   * Bayer run, so they are aliases rather than separate tiles. `EMPTY` is
   * the inverse of `SOLID`; `GREY50` is its own inverse.
   */
  screen_PATTERN_EMPTY  = screen_PATTERN_BAYER0,
  screen_PATTERN_GREY50 = screen_PATTERN_BAYER0 + 32,
  screen_PATTERN_SOLID  = screen_PATTERN_BAYER0 + 64,

  /* The remaining named tiles follow the Bayer run. */
  screen_PATTERN_HSTRIPE = screen_PATTERN_BAYER_LIMIT,
                                 /**< Horizontal bars. */
  screen_PATTERN_VSTRIPE,        /**< Vertical bars. */
  screen_PATTERN_DIAGONAL,       /**< Diagonal lines. */
  screen_PATTERN_DOTS,           /**< Sparse dots. */
  screen_PATTERN_GRID,           /**< Thin grid lines. */
  screen_PATTERN_CROSSHATCH,     /**< Crossed thin lines. */

  screen_PATTERN_HSTRIPE_INV,    /**< Inverse of HSTRIPE. */
  screen_PATTERN_VSTRIPE_INV,    /**< Inverse of VSTRIPE. */
  screen_PATTERN_DIAGONAL_INV,   /**< Inverse of DIAGONAL. */
  screen_PATTERN_DOTS_INV,       /**< Inverse of DOTS. */
  screen_PATTERN_GRID_INV,       /**< Inverse of GRID. */
  screen_PATTERN_CROSSHATCH_INV, /**< Inverse of CROSSHATCH. */

  screen_PATTERN__LIMIT
                    /**< Count of patterns; not itself a pattern. */
}
screen_pattern_t;

/**
 * Fills a box with a repeating 8x8 two-colour pattern.
 *
 * The tile is phased against (`origin_x`, `origin_y`): that coordinate is
 * the one that would map to the box's top-left corner. Passing the caller's
 * own scroll origin keeps the pattern locked to content as the box moves,
 * rather than crawling with it.
 *
 * Clipped to the screen's clip region.
 *
 * \param[in] scr       Screen to draw upon.
 * \param[in] box       Box to fill, inclusive-exclusive.
 * \param[in] pattern   Pattern to fill with.
 * \param[in] origin_x  X coordinate mapping to `box->x0` for tile phase.
 * \param[in] origin_y  Y coordinate mapping to `box->y0` for tile phase.
 * \param[in] fg        Colour for set pattern bits.
 * \param[in] bg        Colour for clear pattern bits.
 */
void screen_fill_pattern(screen_t        *scr,
                         const box_t     *box,
                         screen_pattern_t pattern,
                         int              origin_x,
                         int              origin_y,
                         colour_t         fg,
                         colour_t         bg);

/**
 * Draws a bitmap, alpha-blending it against the screen where the bitmap has
 * an alpha channel. On paletted screens, which have no linear channel bits
 * to blend, this falls back to alpha-tested transparency instead (drawn at
 * full strength, or not at all).
 *
 * The bitmap is clipped to the screen's clip region. No scaling is
 * performed.
 *
 * \param[in] scr  Screen to draw upon.
 * \param[in] x    X coordinate of leftmost point to draw bitmap at.
 * \param[in] y    Y coordinate of topmost point to draw bitmap at.
 * \param[in] src  Bitmap to draw.
 */
void screen_draw_bitmap(screen_t *scr, int x, int y, const bitmap_t *src);

/** Flags for `screen_draw_ninepatch`. */
enum
{
  screen_NINEPATCH_NO_CENTRE = 1u << 0 /**< Leave the interior untouched. */
};

/**
 * Draws a "9-patch": a resizable frame built from a source image that is a
 * 3x3 grid of equal cells. The source width and height must each be a
 * positive multiple of 3; the cell size is a third of each. Given a
 * destination box, the four corner cells are drawn at their natural size in
 * the destination corners, the four edge cells are tiled along the
 * destination edges, and the centre cell is tiled across the interior.
 *
 * If the destination is narrower or shorter than two cells the opposing
 * corners overlap and each is clipped to its own half; the edges and centre
 * are then omitted. Drawing is clipped to both the destination box and the
 * screen's clip region, which is restored on return. Cells are blended
 * exactly as `screen_draw_bitmap` does. No scaling is performed.
 *
 * \param[in] scr   Screen to draw upon.
 * \param[in] dst   Destination box to fill with the frame.
 * \param[in] src   Source image, a 3x3 grid of cells.
 * \param[in] flags Bitwise OR of `screen_NINEPATCH_*`, or 0. Pass
 *                  `screen_NINEPATCH_NO_CENTRE` to draw only the border and
 *                  leave the interior untouched.
 */
void screen_draw_ninepatch(screen_t       *scr,
                           const box_t    *dst,
                           const bitmap_t *src,
                           unsigned int    flags);

/**
 * Copies a rectangular region of the screen to another position on the same
 * screen (e.g. sliding an already-rendered window's pixels to a new position
 * without asking its owner to redraw). Source and destination may overlap;
 * copying is done in the correct row order to handle that safely.
 *
 * Both the source and destination are clipped to the screen's clip region,
 * shrinking together so the copied area always maps source pixel to
 * destination pixel 1:1.
 *
 * Callers must check the return value and fall back to a normal
 * invalidate/redraw when it's false (e.g. out of memory, or an unknown pixel
 * format), since a declined copy leaves the destination untouched.
 *
 * If "src" or the intended destination falls partly off-screen, the actual
 * copied area shrinks to what both ends have in common on-screen: callers
 * must invalidate whatever part of their intended (unclipped) destination
 * falls outside "copied_dst", since it has no valid source pixels to have
 * been copied from and so is left untouched, not merely stale.
 *
 * \param[in]  scr        Screen to copy within.
 * \param[in]  src        Screen-space region to copy from.
 * \param[in]  dst        Top-left of the destination.
 * \param[out] copied_dst Set to the on-screen box actually copied to (may be
 *                        smaller than intended if either end was partly
 *                        off-screen). Pass NULL if not needed. Left unset if
 *                        the copy was declined.
 * \return True if the copy was performed, false if declined (unsupported
 *         pixel format).
 */
int screen_copy_rect(screen_t    *scr,
                     const box_t *src,
                     point_t      dst,
                     box_t       *copied_dst);

/**
 * Draws a line (Bresenham version with aliasing).
 *
 * Coordinates are `int`s. Coordinates are inclusive.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x0      X coordinate of first point of line.
 * \param[in] y0      Y coordinate of first point of line.
 * \param[in] x1      X coordinate of second point of line.
 * \param[in] y1      Y coordinate of second point of line.
 * \param[in] colour  Colour of line.
 */
void screen_draw_line(screen_t *scr,
                      int       x0,
                      int       y0,
                      int       x1,
                      int       y1,
                      colour_t  colour);

/**
 * Draws a connected polyline through `npoints` points: a `screen_draw_line`
 * segment between each adjacent pair. For a closed shape repeat the first
 * point as the last. Fewer than 2 points draws nothing.
 *
 * Segments share their joint pixel, which is plotted by both adjacent
 * segments; harmless for a solid colour.
 *
 * Coordinates are `int`s and inclusive.
 *
 * \param[in] scr      Screen to draw upon.
 * \param[in] points   Array of `npoints` points.
 * \param[in] npoints  Number of points in `points`.
 * \param[in] colour   Colour of the polyline.
 */
void screen_draw_lines(screen_t      *scr,
                       const point_t *points,
                       int            npoints,
                       colour_t       colour);

/**
 * Draws a one-pixel unfilled rectangle outline. `size` is inclusive of both
 * edges, matching `screen_fill_rect`. A degenerate size (<= 1 in either
 * axis) falls back to a filled `screen_fill_rect`.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of leftmost point of rectangle.
 * \param[in] y       Y coordinate of topmost point of rectangle.
 * \param[in] size    Width and height of rectangle.
 * \param[in] colour  Colour of the outline.
 */
void screen_draw_rect(screen_t *scr,
                      int       x,
                      int       y,
                      size2d_t  size,
                      colour_t  colour);

/**
 * Draws a stippled line: `on` pixels drawn, then `off` skipped, repeating
 * along the line. Bresenham stepping, so the dash period is measured in
 * steps not Euclidean distance. `on` <= 0 draws nothing; `off` <= 0 gives a
 * solid line.
 *
 * Coordinates are `int`s and inclusive.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x0      X coordinate of first point of line.
 * \param[in] y0      Y coordinate of first point of line.
 * \param[in] x1      X coordinate of second point of line.
 * \param[in] y1      Y coordinate of second point of line.
 * \param[in] on      Length of each dash, in steps.
 * \param[in] off     Gap between dashes, in steps.
 * \param[in] colour  Colour of line.
 */
void screen_draw_dashed_line(screen_t *scr,
                             int       x0,
                             int       y0,
                             int       x1,
                             int       y1,
                             int       on,
                             int       off,
                             colour_t  colour);

/**
 * Draws a line (fixed-point Wu version with anti-aliasing).
 *
 * Coordinates are fixed point values of type `fix8_t`. Coordinates are
 * inclusive.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x0      X coordinate of first point of line.
 * \param[in] y0      Y coordinate of first point of line.
 * \param[in] x1      X coordinate of second point of line.
 * \param[in] y1      Y coordinate of second point of line.
 * \param[in] colour  Colour of line.
 */
void screen_draw_line_wu_fix8(screen_t *scr,
                              fix8_t    x0,
                              fix8_t    y0,
                              fix8_t    x1,
                              fix8_t    y1,
                              colour_t  colour);

/**
 * Draws a line (floating point Wu version with anti-aliasing).
 *
 * Coordinates are floating point values of type `float`. Coordinates are
 * inclusive.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x0      X coordinate of first point of line.
 * \param[in] y0      Y coordinate of first point of line.
 * \param[in] x1      X coordinate of second point of line.
 * \param[in] y1      Y coordinate of second point of line.
 * \param[in] colour  Colour of rectangle.
 */
void screen_draw_line_wu_float(screen_t *scr,
                               float     x0,
                               float     y0,
                               float     x1,
                               float     y1,
                               colour_t  colour);

#endif /* FRAMEBUF_SCREEN_H */
