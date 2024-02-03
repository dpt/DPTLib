/* screen.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#include "framebuf/screen.h"

void screen_init(screen_t  *scr,
                 int        width,
                 int        height,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 box_t      clip,
                 void      *base)
{
  assert(scr);

  scr->width    = width;
  scr->height   = height;
  scr->format   = fmt;
  scr->rowbytes = rowbytes;
  scr->palette  = palette;
  scr->clip     = clip;
  scr->base     = base;
}

void screen_for_bitmap(screen_t *scr, const bitmap_t *bm)
{
  assert(scr);
  assert(bm);

  memcpy(scr, bm, offsetof(screen_t, clip)); /* copy common members */
  box_reset(&scr->clip);
  scr->base = bm->base;
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

// Pinched from MotionMasks

#define RED_SHIFT   16
#define GREEN_SHIFT 8
#define BLUE_SHIFT  0
#define X_SHIFT     24

#define RED_MASK   (0xFFu << RED_SHIFT)
#define GREEN_MASK (0xFFu << GREEN_SHIFT)
#define BLUE_MASK  (0xFFu << BLUE_SHIFT)
#define X_MASK     (0xFFu << X_SHIFT)

/* Blend specified pixels. */
#define SPAN_ALL8888_BLEND_PIX(fmt, src1, src2, alpha, dst)                      \
{                                                                                \
  fmt r1, g1, b1;                                                                \
  fmt r2, g2, b2;                                                                \
                                                                                 \
  if (alpha == 0)                                                                \
    dst = src1;                                                                  \
  else if (alpha == 255)                                                         \
    dst = src2;                                                                  \
  else                                                                           \
  {                                                                              \
    r1 = (src1 & RED_MASK) >> RED_SHIFT;                                         \
    r2 = (src2 & RED_MASK) >> RED_SHIFT;                                         \
    r1 = (r1 * (256 - alpha) + r2 * alpha) >> 8;                                 \
                                                                                 \
    g1 = (src1 & GREEN_MASK) >> GREEN_SHIFT;                                     \
    g2 = (src2 & GREEN_MASK) >> GREEN_SHIFT;                                     \
    g1 = (g1 * (256 - alpha) + g2 * alpha) >> 8;                                 \
                                                                                 \
    b1 = (src1 & BLUE_MASK) >> BLUE_SHIFT;                                       \
    b2 = (src2 & BLUE_MASK) >> BLUE_SHIFT;                                       \
    b1 = (b1 * (256 - alpha) + b2 * alpha) >> 8;                                 \
                                                                                 \
    dst = (r1 << RED_SHIFT) | (g1 << GREEN_SHIFT) | (b1 << BLUE_SHIFT) | X_MASK; \
  }                                                                              \
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

      SPAN_ALL8888_BLEND_PIX(pixelfmt_bgrx8888_t, scrpx, colpx, alpha, newpx);

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

#define ABS(a) ((a) < 0 ? -(a) : (a))
#define SGN(a) ((a) < 0 ? -1 : 1)
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
  float steep;
  float dx, dy;
  float grad;
  float yf;
  int   xend, yend;
  float xgap;
  int   ix0, ix1;
  int   iy0, iy1;
  int   x;
  int   alpha1, alpha2;
  int   y;

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

  // start point

  xend = roundf(x0);
  yend = y0 + grad * (xend - x0);
  xgap = xend + 0.5 - x0;
  ix0  = xend;
  iy0  = floorf(yend);
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

  // end point

  xend = roundf(x1);
  yend = y1 + grad * (xend - x1);
  xgap = x1 + 0.5f - xend;
  ix1  = xend;
  iy1  = floorf(yend);
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

  for (x = ix0 + 1; x < ix1; x++)
  {
    y = floorf(yf);
    alpha1 = (y + 1.0f - yf) * 255.0f;
    alpha2 = (yf - y) * 255.0f;
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
