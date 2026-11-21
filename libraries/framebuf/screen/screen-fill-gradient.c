/* framebuf/screen/screen-fill-gradient.c -- fill a box with a linear gradient */

#include <float.h>
#include <math.h>

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

/* The step, 0..255, of a pixel whose offset from a, projected onto b - a, is
 * n: n / len2 of the way from a to b. n is a whole number. */
static int gradient_step(double n, double len2)
{
  return CLAMP((int) (n / len2 * 255 + 0.5), 0, 255);
}

void screen_fill_linear_gradient(screen_t    *scr,
                                 const box_t *box,
                                 point_t      a,
                                 point_t      b,
                                 colour_t     c0,
                                 colour_t     c1)
{
  colour_t ramp[256];
  int      i;

  for (i = 0; i < 256; i++)
    ramp[i] = colour_mix_rgba(c0, c1, i, 255);

  screen_fill_linear_gradient_ramp(scr, box, a, b, ramp);
}

void screen_fill_linear_gradient_ramp(screen_t       *scr,
                                      const box_t    *box,
                                      point_t         a,
                                      point_t         b,
                                      const colour_t *ramp)
{
  double         dx, dy, len2;
  box_t          clip_box;
  box_t          draw_box;
  pixelfmt_any_t lut[256];
  int            lo, hi;
  int            i;
  double         n;
  int            step;
  double         first[257];
  int            x, y;
  int            x0;

  dx   = b.x - a.x;
  dy   = b.y - a.y;
  len2 = dx * dx + dy * dy;
  if (len2 == 0)
    return;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (box_intersection(box, &clip_box, &draw_box))
    return; /* nothing visible */

  for (i = 0; i < 256; i++)
    lut[i] = screen_colour_to_pixel(scr, ramp[i]);

  /* The step never falls as n rises, so first[k], the least n reaching step
   * k, bounds each step: a pixel takes step k exactly when
   * first[k] <= n < first[k + 1]. Estimate each, then settle it against
   * gradient_step itself. Pixels then find their step by comparing, not
   * dividing. n is linear in x and y, so the box's corners hold its least
   * and greatest steps, and only the bounds between those are wanted. */
  lo = 255;
  hi = 0;
  for (i = 0; i < 4; i++)
  {
    n    = ((i & 1 ? draw_box.x1 - 1 : draw_box.x0) - a.x) * dx +
           ((i & 2 ? draw_box.y1 - 1 : draw_box.y0) - a.y) * dy;
    step = gradient_step(n, len2);
    lo   = MIN(lo, step);
    hi   = MAX(hi, step);
  }

  first[lo]     = -DBL_MAX;
  first[hi + 1] = DBL_MAX;
  for (i = lo + 1; i <= hi; i++)
  {
    n = ceil((i - 0.5) * len2 / 255);
    while (gradient_step(n, len2) < i)
      n++;
    while (gradient_step(n - 1, len2) >= i)
      n--;
    first[i] = n;
  }

  /* one rect fill per run of pixels taking the same step */
  for (y = draw_box.y0; y < draw_box.y1; y++)
  {
    x0   = draw_box.x0;
    step = -1;
    n    = (x0 - a.x) * dx + (y - a.y) * dy;
    i    = gradient_step(n, len2);
    for (x = draw_box.x0; x <= draw_box.x1; x++, n += dx)
    {
      if (x < draw_box.x1)
      {
        while (n >= first[i + 1])
          i++;
        while (n < first[i])
          i--;
        if (i == step)
          continue;
      }

      if (step >= 0)
        screen_fill_rect_value(scr, x0, y, SIZE2D(x - x0, 1), lut[step]);

      x0   = x;
      step = i;
    }
  }
}

/* ----------------------------------------------------------------------- */

/* The step, 0..255, of a pixel whose squared distance from the centre is
 * d2, where the edge's is len2. d2 is a whole number. */
static int radial_step(double d2, double len2)
{
  return CLAMP((int) (sqrt(d2 / len2) * 255 + 0.5), 0, 255);
}

void screen_fill_radial_gradient_ramp(screen_t       *scr,
                                      const box_t    *box,
                                      point_t         a,
                                      point_t         b,
                                      const colour_t *ramp)
{
  double         len2;
  box_t          clip_box;
  box_t          draw_box;
  pixelfmt_any_t lut[256];
  int            i;
  double         first[257];
  double         d2;
  int            y;
  int            x0;
  int            step;
  double         dx;
  int            x;

  len2 = (double) (b.x - a.x) * (b.x - a.x) +
         (double) (b.y - a.y) * (b.y - a.y);
  if (len2 == 0)
    return;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (box_intersection(box, &clip_box, &draw_box))
    return; /* nothing visible */

  for (i = 0; i < 256; i++)
    lut[i] = screen_colour_to_pixel(scr, ramp[i]);

  /* As for the linear gradient, but over the squared distance d2, so that
   * pixels find their step without a square root. The step never falls as
   * d2 rises, so first[k], the least d2 reaching step k, bounds each. */
  first[0]   = -DBL_MAX;
  first[256] = DBL_MAX;
  for (i = 1; i < 256; i++)
  {
    d2 = ceil((i - 0.5) * (i - 0.5) / (255.0 * 255.0) * len2);
    while (radial_step(d2, len2) < i)
      d2++;
    while (radial_step(d2 - 1, len2) >= i)
      d2--;
    first[i] = d2;
  }

  /* one rect fill per run of pixels taking the same step. along a row d2
   * falls then rises, so the step walks down then up */
  for (y = draw_box.y0; y < draw_box.y1; y++)
  {
    x0   = draw_box.x0;
    step = -1;
    dx   = x0 - a.x;
    d2   = dx * dx + (double) (y - a.y) * (y - a.y);
    i    = radial_step(d2, len2);
    for (x = draw_box.x0; x <= draw_box.x1; x++, d2 += 2 * dx + 1, dx++)
    {
      if (x < draw_box.x1)
      {
        while (d2 >= first[i + 1])
          i++;
        while (d2 < first[i])
          i--;
        if (i == step)
          continue;
      }

      if (step >= 0)
        screen_fill_rect_value(scr, x0, y, SIZE2D(x - x0, 1), lut[step]);

      x0   = x;
      step = i;
    }
  }
}

/* vim: set ts=8 sts=2 sw=2 et: */
