/* screen-draw.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/span-registry.h"
#include "geom/line.h"
#include "utils/fxp.h"

#include "framebuf/screen.h"

/* Number of pixels converted per span call in screen_draw_bitmap(). Bounds
 * the size of its stack scratch buffers so arbitrarily wide bitmaps don't
 * blow the stack (relevant on RISC OS). */
#define BITMAP_BLIT_CHUNK 256

void screen_draw_pixel(screen_t *scr, int x, int y, colour_t colour)
{
  box_t          clip;
  pixelfmt_any_t pxl;

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  pxl = colour_to_pixel(scr->palette,
                        (scr->format == pixelfmt_p4) ? 16 : 0,
                        colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      unsigned char *scrp;
      int            shift;

      scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 1);
      shift = (x & 1) * 4;

      *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((pxl & 0xF) << shift));
    }
    break;

  case 3:
    {
      pixelfmt_any8_t *scrp;

      scrp = scr->base;
      scrp += y * scr->rowbytes / sizeof(*scrp) + x;

      *scrp = (pixelfmt_any8_t) pxl;
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
    assert(!"Unimplemented pixel format");
    break;
  }
}

static void screen_blend_pixel(screen_t *scr,
                               int       x,
                               int       y,
                               colour_t  colour,
                               int       alpha)
{
  box_t clip;

  assert(alpha >= 0);
  assert(alpha <= 255);

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      unsigned char *scrp;
      int            shift;
      unsigned char  idx, out;

      scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 1);
      shift = (x & 1) * 4;
      idx   = (*scrp >> shift) & 0xF;

      scr->span->blendconst(&out, &idx, &colour, 1, alpha, scr->palette);

      *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((out & 0xF) << shift));
    }
    break;

  case 5:
    {
      pixelfmt_any32_t *scrp;
      pixelfmt_any_t    colpx;

      colpx = colour_to_pixel(NULL, 0, colour, scr->format);

      scrp = scr->base;
      scrp += y * scr->rowbytes / sizeof(*scrp) + x;

      scr->span->blendconst(scrp, scrp, &colpx, 1, alpha, NULL);
    }
    break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}

/* ----------------------------------------------------------------------- */

void screen_draw_rect(screen_t *scr,
                      int       x,
                      int       y,
                      size2d_t  size,
                      colour_t  colour)
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
  rect_box.x1 = x + size.w;
  rect_box.y1 = y + size.h;
  if (box_intersection(&clip_box, &rect_box, &draw_box))
    return;

  clipped_width  = draw_box.x1 - draw_box.x0;
  clipped_height = draw_box.y1 - draw_box.y0;

  fmt = colour_to_pixel(scr->palette,
                        (scr->format == pixelfmt_p4) ? 16 : 0,
                        colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      unsigned char *rowp;
      int            yy, xx;

      rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
      for (yy = 0; yy < clipped_height; yy++)
      {
        for (xx = 0; xx < clipped_width; xx++)
        {
          int            x;
          unsigned char *scrp;
          int            shift;

          x     = draw_box.x0 + xx;
          scrp  = rowp + (x >> 1);
          shift = (x & 1) * 4;

          *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((fmt & 0xF) << shift));
        }
        rowp += scr->rowbytes;
      }
    }
    break;

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
    assert(!"Unimplemented pixel format");
    break;
  }
}

void screen_draw_square(screen_t *scr, int x, int y, int size, colour_t colour)
{
  screen_draw_rect(scr, x, y, SIZE2D(size, size), colour);
}

/* ----------------------------------------------------------------------- */

void screen_draw_bitmap(screen_t *scr, int x, int y, const bitmap_t *src)
{
  box_t clip_box;
  box_t src_box;
  box_t draw_box;
  int   clipped_width, clipped_height;
  int   has_alpha;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  src_box.x0 = x;
  src_box.y0 = y;
  src_box.x1 = x + src->size.w;
  src_box.y1 = y + src->size.h;
  if (box_intersection(&clip_box, &src_box, &draw_box))
    return; /* nothing visible */

  clipped_width  = draw_box.x1 - draw_box.x0;
  clipped_height = draw_box.y1 - draw_box.y0;

  /* Source pixels loaded from PNG are always laid out R,G,B,A/X byte order
   * (see bitmap_load_png()), the same layout colour_t::primary uses, so
   * source pixels can be read directly into a colour_t with no conversion. */
  has_alpha = (src->format == pixelfmt_rgba8888 || src->format == pixelfmt_bgra8888);

  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      /* Paletted screen: no linear channel bits to blend, so fall back to
       * alpha-tested (skip fully transparent, else nearest palette match)
       * rather than true blending, matching screen_draw_pixel's case 2. */
      const unsigned char *srcrow;
      unsigned char        *dstbase;
      int                   yy;

      srcrow  = (const unsigned char *) src->base + (draw_box.y0 - y) * src->rowbytes;
      dstbase = scr->base;

      for (yy = 0; yy < clipped_height; yy++)
      {
        const pixelfmt_rgba8888_t *srcpx;
        unsigned char              *rowp;
        int                         xx;

        srcpx = (const pixelfmt_rgba8888_t *) srcrow + (draw_box.x0 - x);
        rowp  = dstbase + (draw_box.y0 + yy) * scr->rowbytes;

        for (xx = 0; xx < clipped_width; xx++)
        {
          colour_t       c;
          int            dstx;
          unsigned char *scrp;
          int            shift;
          pixelfmt_any_t pxl;

          c.primary = srcpx[xx];
          if (has_alpha && colour_get_alpha(&c) == 0)
            continue; /* fully transparent: leave background alone */

          dstx  = draw_box.x0 + xx;
          scrp  = rowp + (dstx >> 1);
          shift = (dstx & 1) * 4;
          pxl   = colour_to_pixel(scr->palette, 16, c, scr->format);

          *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((pxl & 0xF) << shift));
        }

        srcrow += src->rowbytes;
      }
    }
    break;

  case 5:
    {
      pixelfmt_any32_t     colbuf[BITMAP_BLIT_CHUNK];
      unsigned char        alphabuf[BITMAP_BLIT_CHUNK];
      const unsigned char *srcrow;
      pixelfmt_any32_t    *dstrow;
      int                  yy;

      srcrow = (const unsigned char *) src->base + (draw_box.y0 - y) * src->rowbytes;
      dstrow = scr->base;
      dstrow += draw_box.y0 * scr->rowbytes / (int) sizeof(*dstrow) + draw_box.x0;

      for (yy = 0; yy < clipped_height; yy++)
      {
        const pixelfmt_rgba8888_t *srcpx;
        pixelfmt_any32_t          *dstpx;
        int                        remaining;

        srcpx     = (const pixelfmt_rgba8888_t *) srcrow + (draw_box.x0 - x);
        dstpx     = dstrow;
        remaining = clipped_width;

        while (remaining > 0)
        {
          int chunk, i;

          chunk = (remaining > BITMAP_BLIT_CHUNK) ? BITMAP_BLIT_CHUNK : remaining;

          for (i = 0; i < chunk; i++)
          {
            colour_t c;

            c.primary   = srcpx[i];
            colbuf[i]   = colour_to_pixel(scr->palette, 0, c, scr->format);
            alphabuf[i] = has_alpha ? colour_get_alpha(&c) : PIXELFMT_OPAQUE;
          }

          scr->span->blendarray(dstpx, dstpx, colbuf, chunk, alphabuf);

          srcpx     += chunk;
          dstpx     += chunk;
          remaining -= chunk;
        }

        srcrow += src->rowbytes;
        dstrow += scr->rowbytes / (int) sizeof(*dstrow);
      }
    }
    break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}

/* ----------------------------------------------------------------------- */

/* Build the box covering the whole screen, ignoring the current clip
 * rectangle. Unlike the clip rectangle this is invariant across redraws, so
 * clipping a line's endpoints against it yields the same result every time.
 */
static void screen_get_bounds(const screen_t *scr, box_t *bounds)
{
  bounds->x0 = 0;
  bounds->y0 = 0;
  bounds->x1 = scr->size.w;
  bounds->y1 = scr->size.h;
}

void screen_draw_line(screen_t *scr,
                      int       x0,
                      int       y0,
                      int       x1,
                      int       y1,
                      colour_t  colour)
{
  box_t clip_box;
  box_t bounds;
  int   rx0, ry0, rx1, ry1;
  int   dx, dy;
  int   adx, ady;
  int   sx, sy;
  int   error, e2;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  /* Reject only: the clipped-back endpoints are discarded, since feeding
   * them into the stepping maths below would make the pixels chosen depend
   * on which clip rectangle we happened to be called with. */
  rx0 = x0;
  ry0 = y0;
  rx1 = x1;
  ry1 = y1;
  if (line_clip(&clip_box, &rx0, &ry0, &rx1, &ry1) == 0)
    return;

  /* Bound the number of steps taken. Safe to feed into the stepping maths
   * as the screen bounds never vary between calls. Cannot reject: the clip
   * box is always a subset of the screen bounds and it just accepted. */
  screen_get_bounds(scr, &bounds);
  (void) line_clip(&bounds, &x0, &y0, &x1, &y1);

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
                              fix8_t    x0_f8,
                              fix8_t    y0_f8,
                              fix8_t    x1_f8,
                              fix8_t    y1_f8,
                              colour_t  colour)
{
  box_t   clip_box_f8;
  box_t   bounds_f8;
  fix8_t  rx0_f8, ry0_f8, rx1_f8, ry1_f8;
  fix8_t  dx_f8, dy_f8;
  int     steep_b; /* a bool */
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

  /* Reject only: see screen_draw_line() for why the clipped-back endpoints
   * are discarded rather than used. */
  rx0_f8 = x0_f8;
  ry0_f8 = y0_f8;
  rx1_f8 = x1_f8;
  ry1_f8 = y1_f8;
  if (line_clip(&clip_box_f8, &rx0_f8, &ry0_f8, &rx1_f8, &ry1_f8) == 0)
    return;

  /* Bound the number of steps taken, using the invariant screen bounds. */
  screen_get_bounds(scr, &bounds_f8);
  box_scalelog2(&bounds_f8, FIX8_SHIFT);
  (void) line_clip(&bounds_f8, &x0_f8, &y0_f8, &x1_f8, &y1_f8);

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

  /* 64-bit intermediates: FIX16_ONE * dy_f8 and grad_f16 * dx overflow int. */
  grad_f16 = (dx_f8 == 0) ? FIX16_ONE : (fix16_t) ((long long) FIX16_ONE * dy_f8 / dx_f8);

  /* start point */

  xend_i   = FIX8_ROUND_TO_INT(x0_f8);
  yend_f8  = y0_f8 + (fix8_t) ((long long) grad_f16 * (INT_TO_FIX8(xend_i) - x0_f8) / FIX16_ONE);
  xgap_f8  = INT_TO_FIX8(xend_i) + FIX8_ONE / 2 - x0_f8;
  assert(xgap_f8 >= 0 && xgap_f8 <= FIX8_ONE);
  ix0_i    = xend_i;
  iy0_i    = FIX8_FLOOR_TO_INT(yend_f8);
  /* iy0_i may be negative; use multiply not INT_TO_FIX8's left shift. */
  alpha1_i = (255 *  (iy0_i * FIX8_ONE + FIX8_ONE - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
  alpha2_i = (255 * -(iy0_i * FIX8_ONE            - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
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

  /* yend_f8 may be negative; form the fix16 sum by multiply (left-shifting a
   * negative is UB) then arithmetic-shift back down. */
  yf_f8 = (yend_f8 * (FIX16_ONE / FIX8_ONE) + grad_f16) >> (FIX16_SHIFT - FIX8_SHIFT);

  /* end point */

  xend_i   = FIX8_ROUND_TO_INT(x1_f8);
  yend_f8  = y1_f8 + (fix8_t) ((long long) grad_f16 * (INT_TO_FIX8(xend_i) - x1_f8) / FIX16_ONE);
  xgap_f8  = x1_f8 + FIX8_ONE / 2 - INT_TO_FIX8(xend_i);
  assert(xgap_f8 >= 0 && xgap_f8 < FIX8_ONE);
  ix1_i    = xend_i;
  iy1_i    = FIX8_FLOOR_TO_INT(yend_f8);
  alpha1_i = (255 *  (iy1_i * FIX8_ONE + FIX8_ONE - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
  alpha2_i = (255 * -(iy1_i * FIX8_ONE            - yend_f8) * xgap_f8 / FIX8_ONE) / FIX8_ONE;
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
    alpha1_i = (255 *  (y_i * FIX8_ONE + FIX8_ONE - yf_f8)) / FIX8_ONE;
    alpha2_i = (255 * -(y_i * FIX8_ONE            - yf_f8)) / FIX8_ONE;
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
    yf_f8 = (yf_f8 * (FIX16_ONE / FIX8_ONE) + grad_f16) >> (FIX16_SHIFT - FIX8_SHIFT);
  }
}

/* This is a replacement for C99's lroundf(). */
static int my_lroundf(float x)
{
  return (int)(x + (x >= 0 ? 0.5f : -0.5f));
}

void screen_draw_line_wu_float(screen_t *scr,
                               float     fx0,
                               float     fy0,
                               float     fx1,
                               float     fy1,
                               colour_t  colour)
{
  box_t clip_box;
  box_t bounds;
  int   x0, y0, x1, y1;
  float dx, dy;
  int   steep; /* bool */
  float grad;
  int   xend;
  float yend;
  float xgap;
  int   ix0, iy0;
  int   alpha1, alpha2;
  float yf;
  int   ix1, iy1;
  int   xlo, xhi;
  int   xstart, xstop;
  int   x, y;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  /* This discards the fractional part of the coordinates so for now just use it
   * to discard lines. screen_draw_pixel() will be doing clipping too later. */
  x0 = fx0;
  y0 = fy0;
  x1 = fx1;
  y1 = fy1;
  if (line_clip(&clip_box, &x0, &y0, &x1, &y1) == 0)
    return;

  screen_get_bounds(scr, &bounds);

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

  xend   = (int) my_lroundf(fx0);
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

  xend   = (int) my_lroundf(fx1);
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

  /* Bound the loop to the screen. Skipped steps are fast-forwarded through
   * the gradient in closed form, so the pixels drawn stay a function of the
   * true endpoints alone: the screen bounds, unlike the clip box, are the
   * same on every call. */
  xlo    = steep ? bounds.y0 : bounds.x0;
  xhi    = steep ? bounds.y1 : bounds.x1;
  xstart = MAX(ix0 + 1, xlo - 1);
  xstop  = MIN(ix1, xhi + 1);

  yf += grad * (float) (xstart - (ix0 + 1));

  for (x = xstart; x < xstop; x++)
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

