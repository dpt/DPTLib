/* screen.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>

#include "base/utils.h"
#include "framebuf/span-registry.h"

#include "framebuf/screen.h"

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
  box_t          clip;
  pixelfmt_any_t pxl;

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  pxl = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *scrp;

      scrp = scr->base;
      scrp += y * scr->rowbytes / sizeof(*scrp) + x;

      *scrp = pxl;
    }
    break;

  default:
    assert("Unimplemented pixel format" == NULL);
    break;
  }
}

static void screen_blend_pixel(screen_t *scr,
                               int x, int y,
                               colour_t colour, int alpha)
{
  box_t          clip;
  pixelfmt_any_t colpx;

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  colpx = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (scr->format)
  {
  case pixelfmt_bgrx8888:
    {
      pixelfmt_bgrx8888_t *scrp;

      scrp = scr->base;
      scrp += y * scr->rowbytes / sizeof(*scrp) + x;

      scr->span->blendconst(scrp, scrp, &colpx, 1, alpha);
    }
    break;

  default:
    assert("Unimplemented pixel format" == NULL);
    break;
  }
}

/* ----------------------------------------------------------------------- */


void screen_draw_rect(screen_t *scr,
                      int x, int y,
                      int width, int height,
                      colour_t colour)
{
  box_t          clip_box;
  box_t          rect_box;
  box_t          draw_box;
  int            clipped_width, clipped_height;
  pixelfmt_any_t fmt;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  rect_box.x0 = x;
  rect_box.y0 = y;
  rect_box.x1 = x + width;
  rect_box.y1 = y + height;
  if (box_intersection(&clip_box, &rect_box, &draw_box))
    return;

  clipped_width  = draw_box.x1 - draw_box.x0;
  clipped_height = draw_box.y1 - draw_box.y0;

  fmt = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *scrp;
      int               w;

      scrp = scr->base;
      scrp += draw_box.y0 * scr->rowbytes / sizeof(*scrp) + draw_box.x0;
      while (clipped_height--)
      {
        for (w = clipped_width; w > 0; w--)
          *scrp++ = fmt;
        scrp += scr->rowbytes / sizeof(*scrp) - clipped_width;
      }
    }
    break;

  default:
    assert("Unimplemented pixel format" == NULL);
    break;
  }
}

void screen_draw_square(screen_t *scr, int x, int y, int size, colour_t colour)
{
  screen_draw_rect(scr, x, y, size, size, colour);
}

/* ----------------------------------------------------------------------- */

/* Cohen-Sutherland line clipping algorithm */

typedef unsigned int outcode_t;

#define outcode_INSIDE (0)
#define outcode_LEFT   (1)
#define outcode_RIGHT  (2)
#define outcode_BOTTOM (4)
#define outcode_TOP    (8)

static INLINE outcode_t compute_outcode(const box_t *clip, int x, int y)
{
  outcode_t code;

  code = outcode_INSIDE;

  if (x < clip->x0)
    code |= outcode_LEFT;
  else if (x >= clip->x1)
    code |= outcode_RIGHT;

  if (y < clip->y0)
    code |= outcode_BOTTOM;
  else if (y >= clip->y1)
    code |= outcode_TOP;

  return code;
}

static int screen_clip_line(const box_t *clip,
                            int *px0, int *py0, int *px1, int *py1)
{
  int       x0, y0, x1, y1;
  outcode_t o0,o1;

  x0 = *px0;
  y0 = *py0;
  x1 = *px1;
  y1 = *py1;

  o0 = compute_outcode(clip, x0, y0);
  o1 = compute_outcode(clip, x1, y1);

  for (;;)
  {
    if ((o0 | o1) == outcode_INSIDE)
    {
      /* both points lie inside clip - draw*/

      *px0 = x0;
      *py0 = y0;
      *px1 = x1;
      *py1 = y1;

      return 1;
    }
    else if ((o0 & o1) != 0)
    {
      /* both points lie outside clip - don't draw */
      return 0;
    }
    else
    {
      outcode_t oc = o1 > o0 ? o1 : o0;
      int       w  = x1 - x0;
      int       h  = y1 - y0;
      int       x,y;

      if (oc & outcode_TOP)
      {
        x = x0 + w * (clip->y1 - 1 - y0) / h;
        y = clip->y1 - 1;
      }
      else if (oc & outcode_BOTTOM)
      {
        x = x0 + w * (clip->y0 - y0) / h;
        y = clip->y0;
      }
      else if (oc & outcode_RIGHT)
      {
        x = clip->x1 - 1;
        y = y0 + h * (clip->x1 - 1 - x0) / w;
      }
      else if (oc & outcode_LEFT)
      {
        x = clip->x0;
        y = y0 + h * (clip->x0 - x0) / w;
      }

      if (oc == o0)
      {
        x0 = x;
        y0 = y;
        o0 = compute_outcode(clip, x0, y0);
      }
      else
      {
        x1 = x;
        y1 = y;
        o1 = compute_outcode(clip, x1, y1);
      }
    }
  }
}

/* ----------------------------------------------------------------------- */

void screen_draw_line(screen_t *scr,
                      int x0, int y0, int x1, int y1,
                      colour_t colour)
{
  box_t clip_box;
  int   dx, dy;
  int   adx, ady;
  int   sx, sy;
  int   error, e2;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (screen_clip_line(&clip_box, &x0, &y0, &x1, &y1) == 0)
    return;

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

void screen_draw_aa_line(screen_t *scr,
                         int x0, int y0, int x1, int y1,
                         colour_t colour)
{
  screen_draw_aa_linef(scr, x0, y0, x1, y1, colour);
}

void screen_draw_aa_linef(screen_t *scr,
                          float fx0, float fy0, float fx1, float fy1,
                          colour_t colour)
{
  box_t clip_box;
  int   x0, y0, x1, y1;
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

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  // this loses the fp precision, so for now just use it to discard lines.
  // screen_draw_pixel will clip too.
  x0 = fx0;
  y0 = fy0;
  x1 = fx1;
  y1 = fy1;
  if (screen_clip_line(&clip_box, &x0, &y0, &x1, &y1) == 0)
    return;

  dx = fx1 - fx0;
  dy = fy1 - fy0;

  steep = fabsf(dy) > fabsf(dx);
  if (steep)
  {
    SWAP(fx0, fy0);
    SWAP(fx1, fy1);
    SWAP(dx, dy);
  }

  if (fx0 > fx1)
  {
    SWAP(fx0, fx1);
    SWAP(fy0, fy1);
  }

  grad = (dx == 0.0f) ? 1.0f : dy / dx;

  /* start point */

  xend   = (int) lroundf(fx0);
  yend   = fy0 + grad * (xend - fx0);
  xgap   = xend + 0.5f - fx0;
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

  /* end point */

  xend   = (int) lroundf(fx1);
  yend   = fy1 + grad * (xend - fx1);
  xgap   = fx1 + 0.5f - xend;
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
