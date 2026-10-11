/* framebuf/screen/screen-draw-thick-line.c -- thick stroked lines */

#include "base/utils.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* Square root by Newton's method. DPTLib proper has no libm dependency, and
 * this runs once per line. */
static double root(double v)
{
  double r;
  int    i;

  if (v <= 0)
    return 0;

  r = (v > 1) ? v : 1;
  for (i = 0; i < 64; i++)
    r = 0.5 * (r + v / r);

  return r;
}

/* Smallest integer >= v, for a v already clamped into int range. */
static int ceil_int(double v)
{
  int i;

  i = (int) v;
  if ((double) i < v)
    i++;

  return i;
}

/* The line becomes a rectangle `width` wide, with butt ends extended half a
 * pixel so both endpoint pixels are covered, matching screen_draw_line's
 * inclusive endpoints. It is filled by scanline: a pixel is set if its centre
 * lies inside. Each row's span comes from intersecting the four edges with
 * the row's centre line, then screen_fill_hline does the rest. */
void screen_draw_thick_line(screen_t *scr,
                            int       x0,
                            int       y0,
                            int       x1,
                            int       y1,
                            int       width,
                            colour_t  colour)
{
  box_t  clip;
  double dx, dy;
  double len;
  double ux, uy;
  double nx, ny;
  double cx[4], cy[4];
  double ymin, ymax;
  int    y, ya, yb;
  int    i, j;
  double yc;
  double xmin, xmax;
  double t;
  int    xs, xe;

  if (width <= 0)
    return;

  if (width == 1)
  {
    screen_draw_line(scr, x0, y0, x1, y1, colour);
    return;
  }

  if (screen_get_clip(scr, &clip))
    return; /* invalid clipped screen */

  if (x0 == x1 && y0 == y1)
  {
    screen_fill_rect(scr, x0 - width / 2, y0 - width / 2,
                     SIZE2D(width, width), colour);
    return;
  }

  dx  = (double) x1 - x0;
  dy  = (double) y1 - y0;
  len = root(dx * dx + dy * dy);

  ux = dx / len;
  uy = dy / len;
  nx = -uy * width * 0.5;
  ny = ux * width * 0.5;

  /* pixel centres, with the butt ends pushed out by half a pixel */
  cx[0] = x0 + 0.5 - ux * 0.5 + nx;
  cy[0] = y0 + 0.5 - uy * 0.5 + ny;
  cx[1] = x1 + 0.5 + ux * 0.5 + nx;
  cy[1] = y1 + 0.5 + uy * 0.5 + ny;
  cx[2] = x1 + 0.5 + ux * 0.5 - nx;
  cy[2] = y1 + 0.5 + uy * 0.5 - ny;
  cx[3] = x0 + 0.5 - ux * 0.5 - nx;
  cy[3] = y0 + 0.5 - uy * 0.5 - ny;

  ymin = MIN(MIN(cy[0], cy[1]), MIN(cy[2], cy[3]));
  ymax = MAX(MAX(cy[0], cy[1]), MAX(cy[2], cy[3]));

  /* rows whose centre y + 0.5 lies in [ymin, ymax), within the clip */
  ya = ceil_int(CLAMP(ymin - 0.5, clip.y0, clip.y1));
  yb = ceil_int(CLAMP(ymax - 0.5, clip.y0, clip.y1));

  for (y = ya; y < yb; y++)
  {
    yc   = y + 0.5;
    xmin = clip.x1;
    xmax = clip.x0;

    for (i = 0; i < 4; i++)
    {
      j = (i + 1) % 4;

      if (cy[i] == cy[j])
        continue;

      if (yc < MIN(cy[i], cy[j]) || yc >= MAX(cy[i], cy[j]))
        continue;

      t = cx[i] + (yc - cy[i]) * (cx[j] - cx[i]) / (cy[j] - cy[i]);

      xmin = MIN(xmin, t);
      xmax = MAX(xmax, t);
    }

    xs = ceil_int(CLAMP(xmin - 0.5, clip.x0, clip.x1));
    xe = ceil_int(CLAMP(xmax - 0.5, clip.x0, clip.x1));

    if (xe > xs)
      screen_fill_hline(scr, xs, y, xe - xs, colour);
  }
}
