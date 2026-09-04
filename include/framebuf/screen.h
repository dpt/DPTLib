/* framebuf/screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "framebuf/pattern.h"
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
 * Fills a horizontal run of `w` pixels starting at (`x`, `y`).
 *
 * A non-positive `w` draws nothing. Clipped to the screen's clip region.
 * This is the per-row primitive `screen_fill_rect` and `screen_draw_circle`
 * build on.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of the leftmost pixel of the run.
 * \param[in] y       Y coordinate of the run.
 * \param[in] w       Length of the run in pixels.
 * \param[in] colour  Colour of the run.
 */
void screen_fill_hline(screen_t *scr, int x, int y, int w, colour_t colour);

/**
 * Fills a box with a repeating 8x8 pattern.
 *
 * A plain pattern paints every pixel in the box, set bits taking
 * `pattern->fg` and clear bits `pattern->bg`. A stencil pattern (one with
 * `pattern_FLAG_STENCIL`) paints only the set-bit pixels. The tile is phased
 * against `pattern->origin`: that coordinate is the one that maps to the
 * box's top-left corner. Passing the caller's own scroll origin keeps the
 * pattern locked to content as the box moves, rather than crawling with it.
 *
 * Clipped to the screen's clip region.
 *
 * \param[in] scr       Screen to draw upon.
 * \param[in] box       Box to fill, inclusive-exclusive.
 * \param[in] pattern   Pattern to fill with.
 */
void screen_fill_pattern(screen_t        *scr,
                         const box_t     *box,
                         const pattern_t *pattern);

/**
 * Copies a bitmap onto the screen, alpha-blending it against the screen
 * where the bitmap has an alpha channel. On paletted screens, which have no
 * linear channel bits to blend, this falls back to alpha-tested transparency
 * instead (drawn at full strength, or not at all).
 *
 * The bitmap is clipped to the screen's clip region. No scaling is
 * performed.
 *
 * \param[in] scr  Screen to draw upon.
 * \param[in] x    X coordinate of leftmost point to draw bitmap at.
 * \param[in] y    Y coordinate of topmost point to draw bitmap at.
 * \param[in] src  Bitmap to copy.
 * \return \ref result_OK on success, \ref result_NOT_SUPPORTED if the
 *         screen's pixel format has no blit path.
 */
result_t screen_copy_bitmap(screen_t       *scr,
                            int             x,
                            int             y,
                            const bitmap_t *src);

/** Flags for `screen_copy_ninepatch`. */
enum
{
  screen_NINEPATCH_NO_CENTRE = 1u << 0 /**< Leave the interior untouched. */
};

/**
 * Copies a "9-patch" onto the screen: a resizable frame built from a source
 * image that is a 3x3 grid of equal cells. The source width and height must
 * each be a positive multiple of 3; the cell size is a third of each. Given
 * a destination box, the four corner cells are drawn at their natural size
 * in the destination corners, the four edge cells are tiled along the
 * destination edges, and the centre cell is tiled across the interior.
 *
 * If the destination is narrower or shorter than two cells the opposing
 * corners overlap and each is clipped to its own half; the edges and centre
 * are then omitted. Drawing is clipped to both the destination box and the
 * screen's clip region, which is restored on return. Cells are blended
 * exactly as `screen_copy_bitmap` does. No scaling is performed.
 *
 * \param[in] scr   Screen to draw upon.
 * \param[in] dst   Destination box to fill with the frame.
 * \param[in] src   Source image, a 3x3 grid of cells.
 * \param[in] flags Bitwise OR of `screen_NINEPATCH_*`, or 0. Pass
 *                  `screen_NINEPATCH_NO_CENTRE` to draw only the border and
 *                  leave the interior untouched.
 * \return \ref result_OK on success, \ref result_NOT_SUPPORTED if the
 *         screen's pixel format has no blit path.
 */
result_t screen_copy_ninepatch(screen_t       *scr,
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
 * invalidate/redraw when it is not \ref result_OK (an unknown pixel format,
 * or the source/destination lying wholly off-screen), since a declined copy
 * leaves the destination untouched.
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
 * \return \ref result_OK if the copy was performed, \ref
 *         result_NOT_SUPPORTED if declined (unsupported pixel format, or
 *         nothing left to copy after clipping).
 */
result_t screen_copy_rect(screen_t    *scr,
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
 * Draws a one-pixel unfilled circle outline (integer midpoint algorithm, no
 * anti-aliasing). Clipped to the screen's clip region. A negative radius
 * draws nothing; a zero radius draws a single pixel at the centre.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] cx      X coordinate of the centre.
 * \param[in] cy      Y coordinate of the centre.
 * \param[in] r       Radius in pixels.
 * \param[in] colour  Colour of the outline.
 */
void screen_draw_circle(screen_t *scr,
                        int       cx,
                        int       cy,
                        int       r,
                        colour_t  colour);

/**
 * Draws a solid filled disc of the given radius. Clipped to the screen's
 * clip region. A negative radius draws nothing; a zero radius draws a single
 * pixel at the centre.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] cx      X coordinate of the centre.
 * \param[in] cy      Y coordinate of the centre.
 * \param[in] r       Radius in pixels.
 * \param[in] colour  Colour of the disc.
 */
void screen_fill_circle(screen_t *scr,
                        int       cx,
                        int       cy,
                        int       r,
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
