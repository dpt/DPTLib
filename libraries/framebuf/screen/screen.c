/* screen.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>

#include "framebuf/screen.h"
#include "framebuf/span-registry.h"

void screen_init(screen_t  *scr,
                 int        width,
                 int        height,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 void      *base)
{
  assert(scr);

  scr->width    = width;
  scr->height   = height;
  scr->format   = fmt;
  scr->rowbytes = rowbytes;
  scr->palette  = palette;
  scr->span     = spanregistry_get(fmt);
  scr->base     = base;
  box_reset(&scr->clip);
}

void screen_for_bitmap(screen_t *scr, const bitmap_t *bm)
{
  assert(scr);
  assert(bm);

  memcpy(scr, bm, sizeof(bitmap_t)); /* copy common members */
  scr->base = bm->base;
  box_reset(&scr->clip);
}

int screen_get_clip(const screen_t *scr, box_t *clip)
{
  clip->x0 = 0;
  clip->y0 = 0;
  clip->x1 = scr->width;
  clip->y1 = scr->height;

  if (box_is_empty(&scr->clip))
    return 0; /* not empty */

  return box_intersection(clip, &scr->clip, clip);
}

/* ----------------------------------------------------------------------- */

void screen_draw_pixel(screen_t *scr, int x, int y, colour_t colour)
{
  pixelfmt_any_t pxl;

  pxl = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *base;

      base = scr->base;
      base += y * scr->rowbytes / sizeof(*base) + x;
      memset_pattern4(base, &pxl, sizeof(*base)); // FIXME: Not portable.
    }
    break;

  default:
    break;
  }
}

static void screen_blend_pixel(screen_t *scr,
                               int x, int y,
                               colour_t colour, int alpha)
{
  pixelfmt_any_t colpx;

  colpx = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (scr->format)
  {
  case pixelfmt_bgrx8888:
    {
      pixelfmt_bgrx8888_t *p;
      pixelfmt_bgrx8888_t  scrpx;
      pixelfmt_bgrx8888_t  newpx;

      p = scr->base;
      p += y * scr->rowbytes / sizeof(*p) + x;

      scrpx = *((pixelfmt_bgrx8888_t *) p);

      scr->span->blendconst(&newpx, &scrpx, &colpx, 1, alpha);

      *((pixelfmt_bgrx8888_t *) p) = newpx;
    }
    break;

  default:
    break;
  }
}

void screen_draw_rect(screen_t *scr,
                      int x, int y,
                      int width, int height,
                      colour_t colour)
{
  box_t          screen_box;
  box_t          rect_box;
  box_t          draw_box;
  int            clipped_width, clipped_height;
  pixelfmt_any_t fmt;

  if (screen_get_clip(scr, &screen_box))
    return; /* invalid clipped screen */

  rect_box.x0 = x;
  rect_box.y0 = y;
  rect_box.x1 = x + width;
  rect_box.y1 = y + height;
  if (box_intersection(&screen_box, &rect_box, &draw_box))
    return;

  clipped_width = draw_box.x1 - draw_box.x0;
  clipped_height = draw_box.y1 - draw_box.y0;

  fmt = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *base;

      base = scr->base;
      base += draw_box.y0 * scr->rowbytes / sizeof(*base) + draw_box.x0;
      while (clipped_height--)
      {
        memset_pattern4(base, &fmt, clipped_width * sizeof(*base)); // FIXME: Not portable.
        base += scr->rowbytes / sizeof(*base);
      }
    }
    break;

  default:
    break;
  }
}

void screen_draw_square(screen_t *scr, int x, int y, int size, colour_t colour)
{
  screen_draw_rect(scr, x, y, size, size, colour);
}

#define ABS(a)    ((a) < 0 ? -(a) : (a))
#define SGN(a)    ((a) < 0 ? -1 : 1)
#define SWAP(a,b) do { __typeof(a) t = a; a = b; b = t; } while (0)

// TODO: Cope with a clip region.
// TODO: Are (x1,y1) exclusive?
void screen_draw_line(screen_t *scr,
                      int x0, int y0, int x1, int y1,
                      colour_t colour)
{
  // box_t          screen_box;
  // box_t          line_box;
  // box_t          draw_box;
  // int            clipped_width, clipped_height;
  // pixelfmt_any_t fmt;
  //
  // if (screen_get_clip(scr, &screen_box))
  //   return; /* invalid clipped screen */
  //
  // line_box.x0 = x0;
  // line_box.y0 = y0;
  // line_box.x1 = x1;
  // line_box.y1 = y1;
  // if (box_intersection(&screen_box, &line_box, &draw_box))
  //   return;
  //
  // clipped_width = draw_box.x1 - draw_box.x0;
  // clipped_height = draw_box.y1 - draw_box.y0;

  int dx, dy;
  int adx, ady;
  int sx, sy;
  int error, e2;

  dx  = x1 - x0;
  adx = ABS(dx);
  sx  = SGN(dx);

  dy  = y1 - y0;
  ady = -ABS(dy);
  sy  = SGN(dy);

  error = adx + ady;

  for (;;)
  {
    screen_draw_pixel(scr, x0, y0, colour);

    if (x0 == x1 && y0 == y1)
      break;

    e2 = 2 * error;
    if (e2 >= ady)
    {
      if (x0 == x1) { break; }
      error += ady;
      x0 += sx;
    }
    if (e2 <= adx)
    {
      if (y0 == y1) { break; }
      error += adx;
      y0 += sy;
    }
  }
}

void screen_draw_aa_linef(screen_t *scr,
                          float x0, float y0, float x1, float y1,
                          colour_t colour)
{
  float dx, dy;
  float steep;
  float grad;
  int   xend;
  float yend;
  float xgap;
  int   ix0, iy0;
  int   alpha1, alpha2;
  float yf;
  int   ix1, iy1;
  int   x, y;

  dx = x1 - x0;
  dy = y1 - y0;

  steep = fabsf(dy) > fabsf(dx);
  if (steep)
  {
    SWAP(x0, y0);
    SWAP(x1, y1);
    SWAP(dx, dy);
  }

  if (x0 > x1)
  {
    SWAP(x0, x1);
    SWAP(y0, y1);
  }

  grad = (dx == 0.0f) ? 1.0f : dy / dx;

  /* start point */

  xend   = (int) lroundf(x0);
  yend   = y0 + grad * (xend - x0);
  xgap   = xend + 0.5f - x0;
  ix0    = xend;
  iy0    = floorf(yend);
  alpha1 = 255.0f *  (iy0 + 1.0f - yend) * xgap;
  alpha2 = 255.0f * -(iy0        - yend) * xgap;
  if (steep)
  {
    screen_blend_pixel(scr, iy0,     ix0, colour, alpha1);
    screen_blend_pixel(scr, iy0 + 1, ix0, colour, alpha2);
  }
  else
  {
    screen_blend_pixel(scr, ix0, iy0,     colour, alpha1);
    screen_blend_pixel(scr, ix0, iy0 + 1, colour, alpha2);
  }

  yf = yend + grad;

  /* end point (inclusive?) */

  xend   = (int) lroundf(x1);
  yend   = y1 + grad * (xend - x1);
  xgap   = x1 + 0.5f - xend;
  ix1    = xend;
  iy1    = floorf(yend);
  alpha1 = 255.0f *  (iy1 + 1.0f - yend) * xgap;
  alpha2 = 255.0f * -(iy1        - yend) * xgap;
  if (steep)
  {
    screen_blend_pixel(scr, iy1,     ix1, colour, alpha1);
    screen_blend_pixel(scr, iy1 + 1, ix1, colour, alpha2);
  }
  else
  {
    screen_blend_pixel(scr, ix1, iy1,     colour, alpha1);
    screen_blend_pixel(scr, ix1, iy1 + 1, colour, alpha2);
  }

  /* mid points */

  for (x = ix0 + 1; x < ix1; x++)
  {
    y      = floorf(yf);
    alpha1 = 255.0f *  (y + 1.0f - yf);
    alpha2 = 255.0f * -(y        - yf);

    if (steep)
    {
      screen_blend_pixel(scr, y,     x, colour, alpha1);
      screen_blend_pixel(scr, y + 1, x, colour, alpha2);
    }
    else
    {
      screen_blend_pixel(scr, x, y,     colour, alpha1);
      screen_blend_pixel(scr, x, y + 1, colour, alpha2);
    }
    yf += grad;
  }
}

void screen_draw_aa_line(screen_t *scr,
                         int x0, int y0, int x1, int y1,
                         colour_t colour)
{
  screen_draw_aa_linef(scr, x0, y0, x1, y1, colour);
}
