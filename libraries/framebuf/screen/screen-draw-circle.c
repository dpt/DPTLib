/* framebuf/screen/screen-draw-circle.c -- circles and rounded rectangles */

#include <stdlib.h>

#include "base/utils.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* One octant of a midpoint circle, mirrored to the other seven. screen_set_pixel
 * clips each plot, so nothing here needs its own bounds check. */
void screen_draw_circle(screen_t *scr,
                        int       cx,
                        int       cy,
                        int       r,
                        colour_t  colour)
{
  int x, y, err;

  if (r < 0)
    return;

  if (r == 0)
  {
    screen_set_pixel(scr, cx, cy, colour);
    return;
  }

  x   = r;
  y   = 0;
  err = 1 - r;

  while (x >= y)
  {
    screen_set_pixel(scr, cx + x, cy + y, colour);
    screen_set_pixel(scr, cx - x, cy + y, colour);
    screen_set_pixel(scr, cx + x, cy - y, colour);
    screen_set_pixel(scr, cx - x, cy - y, colour);
    screen_set_pixel(scr, cx + y, cy + x, colour);
    screen_set_pixel(scr, cx - y, cy + x, colour);
    screen_set_pixel(scr, cx + y, cy - x, colour);
    screen_set_pixel(scr, cx - y, cy - x, colour);

    y++;
    if (err < 0)
    {
      err += 2 * y + 1;
    }
    else
    {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

/* Same octant stepping as the outline, but each step emits a pair of solid
 * horizontal runs (via screen_fill_hline, which clips) rather than eight
 * points. Runs from the two octant families cover every scanline of the
 * disc exactly once. */
void screen_fill_circle(screen_t *scr,
                        int       cx,
                        int       cy,
                        int       r,
                        colour_t  colour)
{
  int x, y, err;

  if (r < 0)
    return;

  if (r == 0)
  {
    screen_set_pixel(scr, cx, cy, colour);
    return;
  }

  x   = r;
  y   = 0;
  err = 1 - r;

  while (x >= y)
  {
    /* the wide pair: rows cy +/- y, spanning -x..+x */
    screen_fill_hline(scr, cx - x, cy + y, 2 * x + 1, colour);
    if (y != 0)
      screen_fill_hline(scr, cx - x, cy - y, 2 * x + 1, colour);

    /* the tall pair: rows cy +/- x, spanning -y..+y; skip while it would
     * fall inside the wide pair's rows to avoid overdraw */
    if (x != y)
    {
      screen_fill_hline(scr, cx - y, cy + x, 2 * y + 1, colour);
      screen_fill_hline(scr, cx - y, cy - x, 2 * y + 1, colour);
    }

    y++;
    if (err < 0)
    {
      err += 2 * y + 1;
    }
    else
    {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

/* ----------------------------------------------------------------------- */

/* Clamp a rounded rectangle's corner radius so opposing corners never cross,
 * and compute its four corner-circle centres: cl/cr for left/right, ct/cb
 * for top/bottom. */
static int rounded_rect_centres(int      x,
                                int      y,
                                size2d_t size,
                                int      r,
                                int     *cl,
                                int     *ct,
                                int     *cr,
                                int     *cb)
{
  r = CLAMP(r, 0, MIN(size.w - 1, size.h - 1) / 2);

  *cl = x + r;
  *ct = y + r;
  *cr = x + size.w - 1 - r;
  *cb = y + size.h - 1 - r;

  return r;
}

/* The circle outline's octant stepping, but each quadrant is plotted about
 * its own corner centre; straight runs join the corners. Where a run meets
 * an arc the shared pixel is plotted twice, harmlessly. */
void screen_draw_rounded_rect(screen_t *scr,
                              int       x,
                              int       y,
                              size2d_t  size,
                              int       r,
                              colour_t  colour)
{
  int cl, ct, cr, cb;
  int px, py, err;

  if (size.w <= 1 || size.h <= 1)
  {
    screen_fill_rect(scr, x, y, size, colour);
    return;
  }

  r = rounded_rect_centres(x, y, size, r, &cl, &ct, &cr, &cb);
  if (r == 0)
  {
    screen_draw_rect(scr, x, y, size, colour);
    return;
  }

  screen_fill_hline(scr, cl, y, cr - cl + 1, colour);
  screen_fill_hline(scr, cl, y + size.h - 1, cr - cl + 1, colour);
  screen_fill_rect(scr, x, ct, SIZE2D(1, cb - ct + 1), colour);
  screen_fill_rect(scr, x + size.w - 1, ct, SIZE2D(1, cb - ct + 1), colour);

  px  = r;
  py  = 0;
  err = 1 - r;

  while (px >= py)
  {
    screen_set_pixel(scr, cr + px, cb + py, colour);
    screen_set_pixel(scr, cl - px, cb + py, colour);
    screen_set_pixel(scr, cr + px, ct - py, colour);
    screen_set_pixel(scr, cl - px, ct - py, colour);
    screen_set_pixel(scr, cr + py, cb + px, colour);
    screen_set_pixel(scr, cl - py, cb + px, colour);
    screen_set_pixel(scr, cr + py, ct - px, colour);
    screen_set_pixel(scr, cl - py, ct - px, colour);

    py++;
    if (err < 0)
    {
      err += 2 * py + 1;
    }
    else
    {
      px--;
      err += 2 * (py - px) + 1;
    }
  }
}

/* A solid band across the rows between the corner centres, then the filled
 * circle's run pairs above and below it, each run widened by the distance
 * between the left and right centres. */
void screen_fill_rounded_rect(screen_t *scr,
                              int       x,
                              int       y,
                              size2d_t  size,
                              int       r,
                              colour_t  colour)
{
  int cl, ct, cr, cb;
  int span;
  int px, py, err;

  if (size.w <= 0 || size.h <= 0)
    return;

  r = rounded_rect_centres(x, y, size, r, &cl, &ct, &cr, &cb);

  screen_fill_rect(scr, x, ct, SIZE2D(size.w, cb - ct + 1), colour);
  if (r == 0)
    return;

  span = cr - cl + 1;
  px   = r;
  py   = 0;
  err  = 1 - r;

  while (px >= py)
  {
    /* the wide pair; row offset 0 is already in the band */
    if (py != 0)
    {
      screen_fill_hline(scr, cl - px, cb + py, span + 2 * px, colour);
      screen_fill_hline(scr, cl - px, ct - py, span + 2 * px, colour);
    }

    /* the tall pair, skipped where it would repeat the wide pair's rows */
    if (px != py)
    {
      screen_fill_hline(scr, cl - py, cb + px, span + 2 * py, colour);
      screen_fill_hline(scr, cl - py, ct - px, span + 2 * py, colour);
    }

    py++;
    if (err < 0)
    {
      err += 2 * py + 1;
    }
    else
    {
      px--;
      err += 2 * (py - px) + 1;
    }
  }
}
