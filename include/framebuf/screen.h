/* screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "geom/box.h"
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
 * \param[in] width     Width of screen in pixels.
 * \param[in] height    Height of screen in pixels.
 * \param[in] fmt       Pixel format of the screen.
 * \param[in] rowbytes  Number of bytes per row of the screen.
 * \param[in] palette   Palette of the screen, or NULL.
 * \param[in] base      Base address of the screen.
 */
void screen_init(screen_t  *scr,
                 int        width,
                 int        height,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 void      *base);

/**
 * Initialize a previously allocated screen structure, for drawing to an existing bitmap.
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
 * \param[in] width   Width of rectangle.
 * \param[in] height  Height of rectangle.
 * \param[in] colour  Colour of rectangle.
 */
void screen_draw_rect(screen_t *scr,
                      int x, int y,
                      int width, int height,
                      colour_t colour);

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
 * Draws a bitmap, alpha-blending it against the screen where the bitmap
 * has an alpha channel. On paletted screens, which have no linear channel
 * bits to blend, this falls back to alpha-tested transparency instead
 * (drawn at full strength, or not at all).
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

/**
 * Copies a rectangular region of the screen to another position on the
 * same screen (e.g. sliding an already-rendered window's pixels to a new
 * position without asking its owner to redraw). Source and destination may
 * overlap; copying is done in the correct row order to handle that safely.
 *
 * Both the source and destination are clipped to the screen's clip region,
 * shrinking together so the copied area always maps source pixel to
 * destination pixel 1:1.
 *
 * Callers must check the return value and fall back to a normal
 * invalidate/redraw when it's false (e.g. out of memory, or an unknown
 * pixel format), since a declined copy leaves the destination untouched.
 *
 * \param[in] scr   Screen to copy within.
 * \param[in] src   Screen-space region to copy from.
 * \param[in] dst_x X coordinate of the top-left of the destination.
 * \param[in] dst_y Y coordinate of the top-left of the destination.
 * \return True if the copy was performed, false if declined (unsupported
 *         pixel format).
 */
int screen_copy_rect(screen_t *scr, const box_t *src, int dst_x, int dst_y);

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
                      int x0, int y0, int x1, int y1,
                      colour_t colour);

/**
 * Draws a line (fixed-point Wu version with anti-aliasing).
 *
 * Coordinates are fixed point values of type `fix8_t`. Coordinates are inclusive.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x0      X coordinate of first point of line.
 * \param[in] y0      Y coordinate of first point of line.
 * \param[in] x1      X coordinate of second point of line.
 * \param[in] y1      Y coordinate of second point of line.
 * \param[in] colour  Colour of line.
 */
void screen_draw_line_wu_fix8(screen_t *scr,
                              fix8_t x0, fix8_t y0, fix8_t x1, fix8_t y1,
                              colour_t colour);

/**
 * Draws a line (floating point Wu version with anti-aliasing).
 *
 * Coordinates are floating point values of type `float`. Coordinates are inclusive.
 *
 * \param[in] scr     Screen to draw upon.
 * \param[in] x0      X coordinate of first point of line.
 * \param[in] y0      Y coordinate of first point of line.
 * \param[in] x1      X coordinate of second point of line.
 * \param[in] y1      Y coordinate of second point of line.
 * \param[in] colour  Colour of rectangle.
 */
void screen_draw_line_wu_float(screen_t *scr,
                               float x0, float y0, float x1, float y1,
                               colour_t colour);

#endif /* FRAMEBUF_SCREEN_H */
