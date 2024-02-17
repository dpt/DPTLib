/* screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "geom/box.h"
#include "utils/fxp.h"

typedef struct screen screen_t;

// define screen origin etc.

struct screen
{
  bitmap_all_MEMBERS;
  box_t clip; /* rectangular clip region, specified in pixels */
};

void screen_init(screen_t  *scr,
                 int        width,
                 int        height,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 void      *base);

void screen_for_bitmap(screen_t *scr, const bitmap_t *bm);

/* Returns if clip is valid. */
int screen_get_clip(const screen_t *scr, box_t *clip);

/// Draws a pixel.
///
/// - Parameters:
///   - scr: Screen to draw upon.
///   - x: X coordinate of pixel to draw.
///   - y: Y coordinate of pixel to draw.
///   - colour: Colour of pixel.
void screen_draw_pixel(screen_t *scr, int x, int y, colour_t colour);

/// Draws a solid rectangle.
///
/// - Parameters:
///   - scr: Screen to draw upon.
///   - x: X coordinate of leftmost point of rectangle.
///   - y: Y coordinate of topmost point of rectangle.
///   - width: Width of rectangle.
///   - height: Height of rectangle.
///   - colour: Colour of rectangle.
void screen_draw_rect(screen_t *scr,
                      int x, int y,
                      int width, int height,
                      colour_t colour);

/// Special case of `screen_draw_rect`.
void screen_draw_square(screen_t *scr,
                        int x, int y,
                        int size,
                        colour_t colour);

/// Draws a line (aliased Bresenham version).
///
/// Coordinates are `int`s. Coordinates are inclusive.
///
/// - Parameters:
///   - scr: Screen to draw upon.
///   - x0: X coordinate of first point of line.
///   - y0: Y coordinate of first point of line.
///   - x1: X coordinate of second point of line.
///   - y1: Y coordinate of second point of line.
///   - colour: Colour of rectangle.
void screen_draw_line(screen_t *scr,
                      int x0, int y0, int x1, int y1,
                      colour_t colour);

/// Draws a line (anti-aliased fixed point Wu version).
///
/// Coordinates are `fix4_t`s. Coordinates are inclusive.
///
/// - Parameters:
///   - scr: Screen to draw upon.
///   - x0: X coordinate of first point of line.
///   - y0: Y coordinate of first point of line.
///   - x1: X coordinate of second point of line.
///   - y1: Y coordinate of second point of line.
///   - colour: Colour of rectangle.
void screen_draw_line_wu_fix4(screen_t *scr,
                              fix4_t x0, fix4_t y0, fix4_t x1, fix4_t y1,
                              colour_t colour);

/// Draws a line (floating point Wu version).
///
/// Coordinates are `floats`s. Coordinates are inclusive.
///
/// - Parameters:
///   - scr: Screen to draw upon.
///   - x0: X coordinate of first point of line.
///   - y0: Y coordinate of first point of line.
///   - x1: X coordinate of second point of line.
///   - y1: Y coordinate of second point of line.
///   - colour: Colour of rectangle.
void screen_draw_line_wu_float(screen_t *scr,
                               float x0, float y0, float x1, float y1,
                               colour_t colour);

#endif /* FRAMEBUF_SCREEN_H */
