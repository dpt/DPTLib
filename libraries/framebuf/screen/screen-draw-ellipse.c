/* framebuf/screen/screen-draw-ellipse.c -- axis-aligned ellipses */

#include <math.h>

#include "base/utils.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* The run [*xa, *xb] of row y covered by the ellipse filling the w by h box
 * at (x, y0): those pixels whose centres lie inside it. Returns false if
 * there are none. */
static int ellipse_run(int x, int y0, int w, int h, int y, int *xa, int *xb)
{
  double rx, ry, cx, dy, half;

  if (w <= 0 || h <= 0)
    return 0;

  rx = w / 2.0;
  ry = h / 2.0;
  cx = x + rx;
  dy = (y + 0.5 - (y0 + ry)) / ry;
  if (dy <= -1.0 || dy >= 1.0)
    return 0;

  half = rx * sqrt(1.0 - dy * dy);
  *xa  = (int) ceil(cx - half - 0.5);
  *xb  = (int) floor(cx + half - 0.5);

  return *xa <= *xb;
}

/* Row by row: the outer run less the run of the ellipse inset by the
 * thickness. Where the inset ellipse misses a row the whole run is drawn. */
void screen_draw_ellipse(screen_t *scr,
                         int       x,
                         int       y,
                         size2d_t  size,
                         int       thickness,
                         colour_t  colour)
{
  int t;
  int py, xa, xb, ia, ib;

  t = MAX(thickness, 1);

  for (py = y; py < y + size.h; py++)
  {
    if (!ellipse_run(x, y, size.w, size.h, py, &xa, &xb))
      continue;

    if (!ellipse_run(x + t, y + t, size.w - 2 * t, size.h - 2 * t, py,
                     &ia, &ib))
    {
      screen_fill_hline(scr, xa, py, xb - xa + 1, colour);
    }
    else
    {
      screen_fill_hline(scr, xa, py, ia - xa, colour);
      screen_fill_hline(scr, ib + 1, py, xb - ib, colour);
    }
  }
}

void screen_fill_ellipse(screen_t *scr,
                         int       x,
                         int       y,
                         size2d_t  size,
                         colour_t  colour)
{
  int py, xa, xb;

  for (py = y; py < y + size.h; py++)
    if (ellipse_run(x, y, size.w, size.h, py, &xa, &xb))
      screen_fill_hline(scr, xa, py, xb - xa + 1, colour);
}
