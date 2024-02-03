/* screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "geom/box.h"

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
                 box_t      clip,
                 void      *base);

void screen_for_bitmap(screen_t *scr, const bitmap_t *bm);

/* Returns if clip is valid. */
int screen_get_clip(const screen_t *scr, box_t *clip);

void screen_draw_pixel(screen_t *scr,
                       int x, int y,
                       colour_t colour);

/// Draws a solid rectangle.
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

/// Special case of above.
void screen_draw_square(screen_t *scr,
                        int x, int y,
                        int size,
                        colour_t colour);

void screen_draw_line(screen_t *scr,
                      int x0, int y0, int x1, int y1,
                      colour_t colour);

void screen_draw_aa_linef(screen_t *scr,
                          float x0, float y0, float x1, float y1,
                          colour_t colour);

void screen_draw_aa_line(screen_t *scr,
                         int x0, int y0, int x1, int y1,
                         colour_t colour);

#endif /* FRAMEBUF_SCREEN_H */
