/* screen.h -- screen type */

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
void screen_draw_pixel(screen_t *scr, int x, int y, colour_t colour);

/**
 * Draws a solid rectangle.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of leftmost point of rectangle.
 * \param[in] y       Y coordinate of topmost point of rectangle.
 * \param[in] size    Width and height of rectangle.
 * \param[in] colour  Colour of rectangle.
 */
void screen_draw_rect(screen_t *scr,
                      int       x,
                      int       y,
                      size2d_t  size,
                      colour_t  colour);

/**
 * Special case of `screen_draw_rect`.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x       X coordinate of leftmost point of rectangle.
 * \param[in] y       Y coordinate of topmost point of rectangle.
 * \param[in] size    Size of rectangle.
 * \param[in] colour  Colour of rectangle.
 */
void screen_draw_square(screen_t *scr,
                        int x, int y,
                        int size,
                        colour_t colour);

/**
 * Draws a bitmap, alpha-blending it against the screen where the bitmap has an
 * alpha channel. On paletted screens, which have no linear channel bits to
 * blend, this falls back to alpha-tested transparency instead (drawn at full
 * strength, or not at all).
 *
 * The bitmap is clipped to the screen's clip region. No scaling is performed.
 *
 * \param[in] scr  Screen to draw upon.
 * \param[in] x    X coordinate of leftmost point to draw bitmap at.
 * \param[in] y    Y coordinate of topmost point to draw bitmap at.
 * \param[in] src  Bitmap to draw.
 */
void screen_draw_bitmap(screen_t *scr, int x, int y, const bitmap_t *src);

/**
 * Draws a "9-patch": a resizable frame built from a source image that is a 3x3
 * grid of equal cells. The source width and height must each be a positive
 * multiple of 3; the cell size is a third of each. Given a destination box, the
 * four corner cells are drawn at their natural size in the destination corners,
 * the four edge cells are tiled along the destination edges, and the centre
 * cell is tiled across the interior.
 *
 * If the destination is narrower or shorter than two cells the opposing corners
 * overlap and each is clipped to its own half; the edges and centre are then
 * omitted. Drawing is clipped to both the destination box and the screen's clip
 * region, which is restored on return. Cells are blended exactly as
 * `screen_draw_bitmap` does. No scaling is performed.
 *
 * \param[in] scr  Screen to draw upon.
 * \param[in] dst  Destination box to fill with the frame.
 * \param[in] src  Source image, a 3x3 grid of cells.
 */
void screen_draw_ninepatch(screen_t       *scr,
                           const box_t    *dst,
                           const bitmap_t *src);

/**
 * Copies a rectangular region of the screen to another position on the same
 * screen (e.g. sliding an already-rendered window's pixels to a new position
 * without asking its owner to redraw). Source and destination may overlap;
 * copying is done in the correct row order to handle that safely.
 *
 * Both the source and destination are clipped to the screen's clip region,
 * shrinking together so the copied area always maps source pixel to destination
 * pixel 1:1.
 *
 * Callers must check the return value and fall back to a normal
 * invalidate/redraw when it's false (e.g. out of memory, or an unknown pixel
 * format), since a declined copy leaves the destination untouched.
 *
 * If "src" or the intended destination falls partly off-screen, the actual
 * copied area shrinks to what both ends have in common on-screen: callers must
 * invalidate whatever part of their intended (unclipped) destination falls
 * outside "copied_dst", since it has no valid source pixels to have been copied
 * from and so is left untouched, not merely stale.
 *
 * \param[in]  scr        Screen to copy within.
 * \param[in]  src        Screen-space region to copy from.
 * \param[in]  dst        Top-left of the destination.
 * \param[out] copied_dst Set to the on-screen box actually copied to (may be
 *                        smaller than intended if either end was partly
 *                        off-screen). Pass NULL if not needed. Left unset if
 *                        the copy was declined.
 * \return True if the copy was performed, false if declined (unsupported pixel
 *         format).
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
