/* screen-draw.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "base/utils.h"
#include "framebuf/span-registry.h"
#include "geom/line.h"
#include "utils/fxp.h"

#include "framebuf/screen.h"

void screen_draw_pixel(screen_t *scr, int x, int y, colour_t colour)
{
  box_t          clip;
  pixelfmt_any_t pxl;

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  pxl = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 3:
    {
      pixelfmt_any8_t *scrp;

      scrp = scr->base;
      scrp += y * scr->rowbytes / sizeof(*scrp) + x;

      *scrp = pxl;
    }
    break;

  case 4:
    {
      pixelfmt_any16_t *scrp;

      scrp = scr->base;
      scrp += y * scr->rowbytes / sizeof(*scrp) + x;

      *scrp = pxl;
    }
    break;

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

  assert(alpha >= 0);
  assert(alpha <= 255);

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  colpx = colour_to_pixel(NULL, 0, colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 5:
    {
      pixelfmt_any32_t *scrp;

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

  if (line_clip(&clip_box, &x0, &y0, &x1, &y1) == 0)
    return;

  dx  = x1 - x0;
  adx = abs(dx);
  sx  = SGN(dx);

  dy  = y1 - y0;
  ady = -abs(dy);
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

void screen_draw_line_wu_fix8(screen_t *scr,
                              fix8_t x0_f8, fix8_t y0_f8, fix8_t x1_f8, fix8_t y1_f8,
                              colour_t colour)
{
  box_t   clip_box_f8;
  fix8_t  dx_f8, dy_f8;
  int     steep_b; // bool
  fix16_t grad_f16;
  int     xend_i;
  fix8_t  yend_f8;
  fix8_t  xgap_f8;
  int     ix0_i, iy0_i;
  int     alpha1_i, alpha2_i;
  fix8_t  yf_f8;
  int     ix1_i, iy1_i;
  int     x_i, y_i;

  if (screen_get_clip(scr, &clip_box_f8))
    return; /* invalid clipped screen */

  /* scale up screen clip box to match the coordinate type */
  box_scalelog2(&clip_box_f8, FIX8_SHIFT);

  if (line_clip(&clip_box_f8, &x0_f8, &y0_f8, &x1_f8, &y1_f8) == 0)
    return;

  dx_f8 = x1_f8 - x0_f8;
  dy_f8 = y1_f8 - y0_f8;

  steep_b = abs(dy_f8) > abs(dx_f8);
  if (steep_b)
  {
    SWAP(x0_f8, y0_f8);
    SWAP(x1_f8, y1_f8);
    SWAP(dx_f8, dy_f8);
  }

  if (x0_f8 > x1_f8)
  {
    SWAP(x0_f8, x1_f8);
    SWAP(y0_f8, y1_f8);
  }

  grad_f16 = (dx_f8 == 0) ? FIX16_ONE : FIX16_ONE * dy_f8 / dx_f8;

  /* start point */

  xend_i   = FIX8_ROUND_TO_INT(x0_f8);
  yend_f8  = y0_f8 + grad_f16 * (INT_TO_FIX8(xend_i) - x0_f8) / FIX16_ONE;
  xgap_f8  = INT_TO_FIX8(xend_i) + FIX8_ONE / 2 - x0_f8;
  assert(xgap_f8 >= 0 && xgap_f8 <= FIX8_ONE);
  ix0_i    = xend_i;
  iy0_i    = FIX8_FLOOR_TO_INT(yend_f8);
  alpha1_i = (255 *  (INT_TO_FIX8(iy0_i) + FIX8_ONE - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
  alpha2_i = (255 * -(INT_TO_FIX8(iy0_i)            - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
  if (steep_b)
  {
    screen_blend_pixel(scr, iy0_i,     ix0_i, colour, alpha1_i);
    screen_blend_pixel(scr, iy0_i + 1, ix0_i, colour, alpha2_i);
  }
  else
  {
    screen_blend_pixel(scr, ix0_i, iy0_i,     colour, alpha1_i);
    screen_blend_pixel(scr, ix0_i, iy0_i + 1, colour, alpha2_i);
  }

  yf_f8 = ((yend_f8 << (FIX16_SHIFT - FIX8_SHIFT)) + grad_f16) >> (FIX16_SHIFT - FIX8_SHIFT);

  /* end point */

  xend_i   = FIX8_ROUND_TO_INT(x1_f8);
  yend_f8  = y1_f8 + grad_f16 * (INT_TO_FIX8(xend_i) - x1_f8) / FIX16_ONE;
  xgap_f8  = x1_f8 + FIX8_ONE / 2 - INT_TO_FIX8(xend_i);
  assert(xgap_f8 >= 0 && xgap_f8 < FIX8_ONE);
  ix1_i    = xend_i;
  iy1_i    = FIX8_FLOOR_TO_INT(yend_f8);
  alpha1_i = (255 *  (INT_TO_FIX8(iy1_i) + FIX8_ONE - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
  alpha2_i = (255 * -(INT_TO_FIX8(iy1_i)            - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
  if (steep_b)
  {
    screen_blend_pixel(scr, iy1_i,     ix1_i, colour, alpha1_i);
    screen_blend_pixel(scr, iy1_i + 1, ix1_i, colour, alpha2_i);
  }
  else
  {
    screen_blend_pixel(scr, ix1_i, iy1_i,     colour, alpha1_i);
    screen_blend_pixel(scr, ix1_i, iy1_i + 1, colour, alpha2_i);
  }

  /* mid points */

  for (x_i = ix0_i + 1; x_i < ix1_i; x_i++)
  {
    y_i      = FIX8_FLOOR_TO_INT(yf_f8);
    alpha1_i = (255 *  (INT_TO_FIX8(y_i) + FIX8_ONE - yf_f8)) / FIX8_ONE;
    alpha2_i = (255 * -(INT_TO_FIX8(y_i)            - yf_f8)) / FIX8_ONE;
    if (steep_b)
    {
      screen_blend_pixel(scr, y_i,     x_i, colour, alpha1_i);
      screen_blend_pixel(scr, y_i + 1, x_i, colour, alpha2_i);
    }
    else
    {
      screen_blend_pixel(scr, x_i, y_i,     colour, alpha1_i);
      screen_blend_pixel(scr, x_i, y_i + 1, colour, alpha2_i);
    }
    yf_f8 = ((yf_f8 << (FIX16_SHIFT - FIX8_SHIFT)) + grad_f16) >> (FIX16_SHIFT - FIX8_SHIFT);
  }
}

void screen_draw_line_wu_float(screen_t *scr,
                               float fx0, float fy0, float fx1, float fy1,
                               colour_t colour)
{
  box_t clip_box;
  int   x0, y0, x1, y1;
  float dx, dy;
  int   steep; // bool
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
  if (line_clip(&clip_box, &x0, &y0, &x1, &y1) == 0)
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
  assert(xgap >= 0.0f && xgap <= 1.0f);
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
  assert(xgap >= 0.0f && xgap < 1.0f);
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

