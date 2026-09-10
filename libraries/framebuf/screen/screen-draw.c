/* framebuf/screen/screen-draw.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pattern.h"
#include "framebuf/pixelmap.h"
#include "framebuf/span-registry.h"
#include "geom/line.h"
#include "utils/fxp.h"

#include "framebuf/screen.h"

#include "screen-copy-bitmap-rle.h"

/* Number of pixels converted per span call in screen_copy_bitmap(). Bounds
 * the size of its stack scratch buffers so arbitrarily wide bitmaps don't
 * blow the stack (relevant on RISC OS). */
#define BITMAP_BLIT_CHUNK 256

/* Each helper writes the single pixel (x, y), already known to be inside the
 * clip, with the colour previously resolved to "pxl". */

static void screen_set_pixel_p1(screen_t      *scr,
                                int            x,
                                int            y,
                                pixelfmt_any_t pxl)
{
  unsigned char *scrp;
  int            shift;

  scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 3);
  shift = 7 - (x & 7); /* bit 7 is the leftmost pixel */

  *scrp = (unsigned char) ((*scrp & ~(1 << shift)) | ((pxl & 1) << shift));
}

static void screen_set_pixel_p2(screen_t      *scr,
                                int            x,
                                int            y,
                                pixelfmt_any_t pxl)
{
  unsigned char *scrp;
  int            shift;

  scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 2);
  shift = 6 - ((x & 3) << 1); /* bits 7..6 are the leftmost pixel */

  *scrp = (unsigned char) ((*scrp & ~(3 << shift)) | ((pxl & 3) << shift));
}

static void screen_set_pixel_p4(screen_t      *scr,
                                int            x,
                                int            y,
                                pixelfmt_any_t pxl)
{
  unsigned char *scrp;
  int            shift;

  scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 1);
  shift = (x & 1) * 4;

  *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((pxl & 0xF) << shift));
}

static void screen_set_pixel_p8(screen_t      *scr,
                                int            x,
                                int            y,
                                pixelfmt_any_t pxl)
{
  pixelfmt_any8_t *scrp;

  scrp = scr->base;
  scrp += y * scr->rowbytes / sizeof(*scrp) + x;

  *scrp = (pixelfmt_any8_t) pxl;
}

static void screen_set_pixel_16(screen_t      *scr,
                                int            x,
                                int            y,
                                pixelfmt_any_t pxl)
{
  pixelfmt_any16_t *scrp;

  scrp = scr->base;
  scrp += y * scr->rowbytes / sizeof(*scrp) + x;

  *scrp = pxl;
}

static void screen_set_pixel_32(screen_t      *scr,
                                int            x,
                                int            y,
                                pixelfmt_any_t pxl)
{
  pixelfmt_any32_t *scrp;

  scrp = scr->base;
  scrp += y * scr->rowbytes / sizeof(*scrp) + x;

  *scrp = pxl;
}

void screen_set_pixel(screen_t *scr, int x, int y, colour_t colour)
{
  box_t          clip;
  pixelfmt_any_t pxl;

  if (screen_get_clip(scr, &clip) || !box_contains_point(&clip, x, y))
    return;

  pxl = colour_to_pixel(scr->palette,
                        pixelfmt_paletted_nentries(scr->format),
                        colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 0: screen_set_pixel_p1(scr, x, y, pxl); break;
  case 1: screen_set_pixel_p2(scr, x, y, pxl); break;
  case 2: screen_set_pixel_p4(scr, x, y, pxl); break;
  case 3: screen_set_pixel_p8(scr, x, y, pxl); break;
  case 4: screen_set_pixel_16(scr, x, y, pxl); break;
  case 5: screen_set_pixel_32(scr, x, y, pxl); break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}

/* Each helper alpha-blends "colour" at "alpha" into the single pixel (x, y),
 * already known to be inside the clip. */

static void screen_blend_pixel_p1(screen_t *scr,
                                  int       x,
                                  int       y,
                                  colour_t  colour,
                                  int       alpha)
{
  unsigned char *scrp;
  int            shift;
  unsigned char  idx, out;

  scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 3);
  shift = 7 - (x & 7);
  idx   = (*scrp >> shift) & 1;

  scr->span->blendconst(&out, &idx, &colour, 1, alpha, scr->palette);

  *scrp = (unsigned char) ((*scrp & ~(1 << shift)) | ((out & 1) << shift));
}

static void screen_blend_pixel_p2(screen_t *scr,
                                  int       x,
                                  int       y,
                                  colour_t  colour,
                                  int       alpha)
{
  unsigned char *scrp;
  int            shift;
  unsigned char  idx, out;

  scrp  = (unsigned char *) scr->base + y * scr->rowbytes + (x >> 2);
  shift = 6 - ((x & 3) << 1);
  idx   = (*scrp >> shift) & 3;

  scr->span->blendconst(&out, &idx, &colour, 1, alpha, scr->palette);

  *scrp = (unsigned char) ((*scrp & ~(3 << shift)) | ((out & 3) << shift));
}

static void screen_blend_pixel_p4(screen_t *scr,
                                  int       x,
                                  int       y,
                                  colour_t  colour,
                                  int       alpha)
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

static void screen_blend_pixel_32(screen_t *scr,
                                  int       x,
                                  int       y,
                                  colour_t  colour,
                                  int       alpha)
{
  pixelfmt_any32_t *scrp;
  pixelfmt_any_t    colpx;

  colpx = colour_to_pixel(NULL, 0, colour, scr->format);

  scrp = scr->base;
  scrp += y * scr->rowbytes / sizeof(*scrp) + x;

  scr->span->blendconst(scrp, scrp, &colpx, 1, alpha, NULL);
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
  case 0: screen_blend_pixel_p1(scr, x, y, colour, alpha); break;
  case 1: screen_blend_pixel_p2(scr, x, y, colour, alpha); break;
  case 2: screen_blend_pixel_p4(scr, x, y, colour, alpha); break;
  case 5: screen_blend_pixel_32(scr, x, y, colour, alpha); break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}

/* ----------------------------------------------------------------------- */

void screen_fill_rect(screen_t *scr,
                      int       x,
                      int       y,
                      size2d_t  size,
                      colour_t  colour)
{
  box_t clip_box;
  box_t rect_box;
  box_t draw_box;
  int   yy;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  rect_box.x0 = x;
  rect_box.y0 = y;
  rect_box.x1 = x + size.w;
  rect_box.y1 = y + size.h;
  if (box_intersection(&clip_box, &rect_box, &draw_box))
    return;

  /* Each row is already clipped, so hand the pre-clipped span straight to
   * screen_fill_hline. */
  for (yy = draw_box.y0; yy < draw_box.y1; yy++)
    screen_fill_hline(scr, draw_box.x0, yy, draw_box.x1 - draw_box.x0, colour);
}

void screen_fill_rects(screen_t    *scr,
                       const box_t *boxes,
                       int          nboxes,
                       colour_t     colour)
{
  int i;

  for (i = 0; i < nboxes; i++)
    screen_fill_rect(scr,
                     boxes[i].x0,
                     boxes[i].y0,
                     box_size(&boxes[i]),
                     colour);
}

void screen_fill_square(screen_t *scr,
                        int       x,
                        int       y,
                        int       size,
                        colour_t  colour)
{
  screen_fill_rect(scr, x, y, SIZE2D(size, size), colour);
}

/* ----------------------------------------------------------------------- */

/* Per-blit ordered-dither bias table: one signed offset for each of the 64
 * cells of the 8x8 Bayer matrix, indexed by pattern_bayer_threshold(sx, sy).
 *
 * The mean gap between adjacent levels of a linear ramp of "nlevels" entries
 * is 255 / (nlevels - 1); the Bayer cell (-32..31 about zero) nudges a channel
 * value by up to half that gap either way, so a value sitting between two
 * entries lands on one or the other in a fixed 8x8 pattern instead of always
 * snapping to the nearer. Building the table hoists the one divide out of the
 * inner loop -- the per-pixel cost drops to a lookup and an add.
 *
 * dither_bias_build fills "tab" and returns 1 when dithering is on and useful
 * (>= 2 levels), 0 when the caller should skip the bias entirely.
 *
 * ponytail: this assumes a roughly even, roughly greyscale palette -- the one
 * case (shallow paletted screen showing a gradient) the dithered blit is for.
 * A wildly non-uniform palette dithers weakly, not wrongly; swap in a
 * per-entry nearest-two search keyed on the real palette if that matters. */
static int dither_bias_build(int tab[64], int nlevels, int dither)
{
  int gap;
  int i;

  if (!dither || nlevels < 2)
    return 0;

  gap = 255 / (nlevels - 1);
  for (i = 0; i < 64; i++)
    tab[i] = ((i - 32) * gap) / 64;

  return 1;
}

/* Apply the pre-built bias for screen pixel (sx, sy) to channel value "v"
 * (0..255), result clamped to 0..255. */
static unsigned int dither_channel(unsigned int v,
                                   const int    tab[64],
                                   int          sx,
                                   int          sy)
{
  int adj;

  adj = tab[pattern_bayer_threshold(sx, sy)];

  return (unsigned int) CLAMP((int) v + adj, 0, 255);
}

/* Blit "src" onto the paletted screen, its top-left at (x, y), clipped to
 * "draw_box". No linear channel bits to blend, so this does an alpha-tested
 * transfer (skip fully transparent, else nearest palette match) rather than
 * true blending, matching screen_set_pixel's case 2. With "dither" set the
 * source RGB is ordered-dithered per pixel before the nearest-match lookup,
 * breaking up the banding a shallow palette otherwise shows on a gradient.
 * Returns result_NOT_SUPPORTED if there is no deep->paletted conversion table
 * for the screen's format. */
static result_t screen_copy_bitmap_p4(screen_t       *scr,
                                      int             x,
                                      int             y,
                                      const bitmap_t *src,
                                      const box_t    *draw_box,
                                      int             has_alpha,
                                      int             dither)
{
  const unsigned char *srcrow;
  unsigned char       *dstbase;
  const pixelmap_t    *pm;
  int                  bias[64];
  int                  do_dither;
  int                  clipped_width, clipped_height;
  int                  yy;

  clipped_width  = draw_box->x1 - draw_box->x0;
  clipped_height = draw_box->y1 - draw_box->y0;
  do_dither      = dither_bias_build(bias,
                                    pixelfmt_paletted_nentries(scr->format),
                                    dither);

  srcrow  = (const unsigned char *) src->base + (draw_box->y0 - y) * src->rowbytes;
  dstbase = scr->base;

  /* Source pixels here are RGBA8888 byte order (see screen_copy_bitmap). The
   * cached RGB->index table turns the inner loop into a mask-and-lookup. */
  pm = pixelmap_get(pixelfmt_rgba8888, scr->format, scr->palette, 16);
  if (pm == NULL)
    return result_NOT_SUPPORTED;

  for (yy = 0; yy < clipped_height; yy++)
  {
    const pixelfmt_rgba8888_t *srcpx;
    unsigned char             *rowp;
    int                        xx;

    srcpx = (const pixelfmt_rgba8888_t *) srcrow + (draw_box->x0 - x);
    rowp  = dstbase + (draw_box->y0 + yy) * scr->rowbytes;

    for (xx = 0; xx < clipped_width; xx++)
    {
      colour_t       c;
      unsigned int   r, g, b, idx;
      int            dstx;
      unsigned char *scrp;
      int            shift;
      pixelfmt_any_t pxl;

      c.primary = srcpx[xx];
      if (has_alpha && colour_get_alpha(&c) == 0)
        continue; /* fully transparent: leave background alone */

      dstx  = draw_box->x0 + xx;
      scrp  = rowp + (dstx >> 1);
      shift = (dstx & 1) * 4;

      r   = (c.primary >> pm->rshift) & 0xFF;
      g   = (c.primary >> pm->gshift) & 0xFF;
      b   = (c.primary >> pm->bshift) & 0xFF;
      if (do_dither)
      {
        r = dither_channel(r, bias, dstx, draw_box->y0 + yy);
        g = dither_channel(g, bias, dstx, draw_box->y0 + yy);
        b = dither_channel(b, bias, dstx, draw_box->y0 + yy);
      }
      idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits))
          | ((g >> (8 - pm->gbits)) << pm->bbits)
          | ( b >> (8 - pm->bbits));
      pxl = (pm->entries[idx >> 1] >> ((idx & 1) << 2)) & 0xF;

      *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((pxl & 0xF) << shift));
    }

    srcrow += src->rowbytes;
  }

  return result_OK;
}

/* Blit "src" onto the 1bpp screen, its top-left at (x, y), clipped to
 * "draw_box". Like screen_copy_bitmap_p4 this is an alpha-tested transfer
 * (skip fully transparent, else nearest of the two palette entries) rather
 * than a blend -- a 1bpp screen has no channel bits to blend. "dither"
 * ordered-dithers the source RGB per pixel first. The cached RGB->index table
 * turns the inner loop into a mask-and-lookup; bit 7 of a byte is the
 * leftmost pixel. Returns result_NOT_SUPPORTED if there is no deep->paletted
 * conversion table for the screen's format. */
static result_t screen_copy_bitmap_p1(screen_t       *scr,
                                      int             x,
                                      int             y,
                                      const bitmap_t *src,
                                      const box_t    *draw_box,
                                      int             has_alpha,
                                      int             dither)
{
  const unsigned char *srcrow;
  unsigned char       *dstbase;
  const pixelmap_t    *pm;
  int                  bias[64];
  int                  do_dither;
  int                  clipped_width, clipped_height;
  int                  yy;

  clipped_width  = draw_box->x1 - draw_box->x0;
  clipped_height = draw_box->y1 - draw_box->y0;
  do_dither      = dither_bias_build(bias,
                                    pixelfmt_paletted_nentries(scr->format),
                                    dither);

  srcrow  = (const unsigned char *) src->base + (draw_box->y0 - y) * src->rowbytes;
  dstbase = scr->base;

  pm = pixelmap_get(pixelfmt_rgba8888, scr->format, scr->palette, 2);
  if (pm == NULL)
    return result_NOT_SUPPORTED;

  for (yy = 0; yy < clipped_height; yy++)
  {
    const pixelfmt_rgba8888_t *srcpx;
    unsigned char             *rowp;
    int                        xx;

    srcpx = (const pixelfmt_rgba8888_t *) srcrow + (draw_box->x0 - x);
    rowp  = dstbase + (draw_box->y0 + yy) * scr->rowbytes;

    for (xx = 0; xx < clipped_width; xx++)
    {
      colour_t       c;
      unsigned int   r, g, b, idx;
      int            dstx, shift;
      unsigned char *scrp;
      pixelfmt_any_t pxl;

      c.primary = srcpx[xx];
      if (has_alpha && colour_get_alpha(&c) == 0)
        continue; /* fully transparent: leave background alone */

      dstx  = draw_box->x0 + xx;
      scrp  = rowp + (dstx >> 3);
      shift = 7 - (dstx & 7);

      r   = (c.primary >> pm->rshift) & 0xFF;
      g   = (c.primary >> pm->gshift) & 0xFF;
      b   = (c.primary >> pm->bshift) & 0xFF;
      if (do_dither)
      {
        r = dither_channel(r, bias, dstx, draw_box->y0 + yy);
        g = dither_channel(g, bias, dstx, draw_box->y0 + yy);
        b = dither_channel(b, bias, dstx, draw_box->y0 + yy);
      }
      idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits))
          | ((g >> (8 - pm->gbits)) << pm->bbits)
          | ( b >> (8 - pm->bbits));
      pxl = (pm->entries[idx >> 3] >> (idx & 7)) & 1;

      *scrp = (unsigned char) ((*scrp & ~(1 << shift)) | ((pxl & 1) << shift));
    }

    srcrow += src->rowbytes;
  }

  return result_OK;
}

/* Blit "src" onto the 2bpp screen, its top-left at (x, y), clipped to
 * "draw_box". Alpha-tested transfer to the nearest of the four palette
 * entries, exactly as screen_copy_bitmap_p1 but two bits per pixel: bits
 * 7..6 of a byte are the leftmost pixel and the cached RGB->index table is
 * packed four entries to the byte. "dither" ordered-dithers the source RGB
 * per pixel first. Returns result_NOT_SUPPORTED if there is no deep->paletted
 * conversion table for the screen's format. */
static result_t screen_copy_bitmap_p2(screen_t       *scr,
                                      int             x,
                                      int             y,
                                      const bitmap_t *src,
                                      const box_t    *draw_box,
                                      int             has_alpha,
                                      int             dither)
{
  const unsigned char *srcrow;
  unsigned char       *dstbase;
  const pixelmap_t    *pm;
  int                  bias[64];
  int                  do_dither;
  int                  clipped_width, clipped_height;
  int                  yy;

  clipped_width  = draw_box->x1 - draw_box->x0;
  clipped_height = draw_box->y1 - draw_box->y0;
  do_dither      = dither_bias_build(bias,
                                    pixelfmt_paletted_nentries(scr->format),
                                    dither);

  srcrow  = (const unsigned char *) src->base + (draw_box->y0 - y) * src->rowbytes;
  dstbase = scr->base;

  pm = pixelmap_get(pixelfmt_rgba8888, scr->format, scr->palette, 4);
  if (pm == NULL)
    return result_NOT_SUPPORTED;

  for (yy = 0; yy < clipped_height; yy++)
  {
    const pixelfmt_rgba8888_t *srcpx;
    unsigned char             *rowp;
    int                        xx;

    srcpx = (const pixelfmt_rgba8888_t *) srcrow + (draw_box->x0 - x);
    rowp  = dstbase + (draw_box->y0 + yy) * scr->rowbytes;

    for (xx = 0; xx < clipped_width; xx++)
    {
      colour_t       c;
      unsigned int   r, g, b, idx;
      int            dstx, shift;
      unsigned char *scrp;
      pixelfmt_any_t pxl;

      c.primary = srcpx[xx];
      if (has_alpha && colour_get_alpha(&c) == 0)
        continue; /* fully transparent: leave background alone */

      dstx  = draw_box->x0 + xx;
      scrp  = rowp + (dstx >> 2);
      shift = 6 - ((dstx & 3) << 1);

      r   = (c.primary >> pm->rshift) & 0xFF;
      g   = (c.primary >> pm->gshift) & 0xFF;
      b   = (c.primary >> pm->bshift) & 0xFF;
      if (do_dither)
      {
        r = dither_channel(r, bias, dstx, draw_box->y0 + yy);
        g = dither_channel(g, bias, dstx, draw_box->y0 + yy);
        b = dither_channel(b, bias, dstx, draw_box->y0 + yy);
      }
      idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits))
          | ((g >> (8 - pm->gbits)) << pm->bbits)
          | ( b >> (8 - pm->bbits));
      pxl = (pm->entries[idx >> 2] >> ((idx & 3) << 1)) & 3;

      *scrp = (unsigned char) ((*scrp & ~(3 << shift)) | ((pxl & 3) << shift));
    }

    srcrow += src->rowbytes;
  }

  return result_OK;
}

/* Blit "src" onto the 32bpp screen, its top-left at (x, y), clipped to
 * "draw_box", alpha-blending row spans through the span registry. */
static void screen_copy_bitmap_32(screen_t       *scr,
                                  int             x,
                                  int             y,
                                  const bitmap_t *src,
                                  const box_t    *draw_box,
                                  int             has_alpha)
{
  pixelfmt_any32_t     colbuf[BITMAP_BLIT_CHUNK];
  unsigned char        alphabuf[BITMAP_BLIT_CHUNK];
  const unsigned char *srcrow;
  pixelfmt_any32_t    *dstrow;
  int                  clipped_width, clipped_height;
  int                  yy;

  clipped_width  = draw_box->x1 - draw_box->x0;
  clipped_height = draw_box->y1 - draw_box->y0;

  srcrow = (const unsigned char *) src->base + (draw_box->y0 - y) * src->rowbytes;
  dstrow = scr->base;
  dstrow += draw_box->y0 * scr->rowbytes / (int) sizeof(*dstrow) + draw_box->x0;

  for (yy = 0; yy < clipped_height; yy++)
  {
    const pixelfmt_rgba8888_t *srcpx;
    pixelfmt_any32_t          *dstpx;
    int                        remaining;

    srcpx     = (const pixelfmt_rgba8888_t *) srcrow + (draw_box->x0 - x);
    dstpx     = dstrow;
    remaining = clipped_width;

    while (remaining > 0)
    {
      int chunk, i;

      chunk = MIN(remaining, BITMAP_BLIT_CHUNK);

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

/* Shared body for screen_copy_bitmap and screen_copy_bitmap_dithered. With
 * "dither" set the paletted (p1/p2/p4) paths ordered-dither the source RGB
 * before the nearest-match lookup; the 32bpp path and the RLE path ignore it
 * (a 32bpp screen has the channel depth not to band, and an RLE source is
 * pre-quantised UI art). */
static result_t screen_copy_bitmap_i(screen_t       *scr,
                                     int             x,
                                     int             y,
                                     const bitmap_t *src,
                                     int             dither)
{
  box_t clip_box;
  box_t src_box;
  box_t draw_box;
  int   has_alpha;

  if (screen_get_clip(scr, &clip_box))
    return result_OK; /* invalid clipped screen: nothing to draw */

  src_box.x0 = x;
  src_box.y0 = y;
  src_box.x1 = x + src->size.w;
  src_box.y1 = y + src->size.h;
  if (box_intersection(&clip_box, &src_box, &draw_box))
    return result_OK; /* nothing visible */

  if (pixelfmt_is_rle(src->format))
    return screen_copy_bitmap_rle(scr, x, y, src, &draw_box);

  /* Source pixels loaded from PNG are always laid out R,G,B,A/X byte order
   * (see bitmap_load_png()), the same layout colour_t::primary uses, so
   * source pixels can be read directly into a colour_t with no conversion. */
  has_alpha = (src->format == pixelfmt_rgba8888 || src->format == pixelfmt_bgra8888);

  switch (pixelfmt_log2bpp(scr->format))
  {
  case 0: return screen_copy_bitmap_p1(scr, x, y, src, &draw_box, has_alpha, dither);
  case 1: return screen_copy_bitmap_p2(scr, x, y, src, &draw_box, has_alpha, dither);
  case 2: return screen_copy_bitmap_p4(scr, x, y, src, &draw_box, has_alpha, dither);
  case 5: screen_copy_bitmap_32(scr, x, y, src, &draw_box, has_alpha); break;

  default:
    assert(!"Unimplemented pixel format");
    return result_NOT_SUPPORTED;
  }

  return result_OK;
}

result_t screen_copy_bitmap(screen_t *scr, int x, int y, const bitmap_t *src)
{
  return screen_copy_bitmap_i(scr, x, y, src, 0);
}

result_t screen_copy_bitmap_dithered(screen_t       *scr,
                                     int             x,
                                     int             y,
                                     const bitmap_t *src)
{
  return screen_copy_bitmap_i(scr, x, y, src, 1);
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
    screen_set_pixel(scr, x0, y0, colour);

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

void screen_draw_lines(screen_t      *scr,
                       const point_t *points,
                       int            npoints,
                       colour_t       colour)
{
  int i;

  if (points == NULL || npoints < 2)
    return;

  /* ponytail: per-segment call; joint pixels double-plot, fine for solid fill */
  for (i = 1; i < npoints; i++)
    screen_draw_line(scr,
                     points[i - 1].x, points[i - 1].y,
                     points[i].x,     points[i].y,
                     colour);
}

void screen_draw_rect(screen_t *scr,
                      int       x,
                      int       y,
                      size2d_t  size,
                      colour_t  colour)
{
  int     x1, y1;
  point_t p[5];

  if (size.w <= 1 || size.h <= 1)
  {
    screen_fill_rect(scr, x, y, size, colour);
    return;
  }

  x1 = x + size.w - 1;
  y1 = y + size.h - 1;

  p[0].x = x;  p[0].y = y;
  p[1].x = x1; p[1].y = y;
  p[2].x = x1; p[2].y = y1;
  p[3].x = x;  p[3].y = y1;
  p[4].x = x;  p[4].y = y;

  screen_draw_lines(scr, p, 5, colour);
}

void screen_draw_bevel_edge(screen_t    *scr,
                            const box_t *box,
                            colour_t     a,
                            colour_t     b)
{
  int x0, y0, x1, y1;
  int w, h;
  
  x0 = box->x0;
  y0 = box->y0;
  x1 = box->x1;
  y1 = box->y1;
  w  = x1 - x0;
  h  = y1 - y0;
  
  /* For a 5x5 box draw like so using colours A and B:
   *
   * AAAAB
   * AAABB
   * AA BB
   * AABBB
   * ABBBB
   *
   * Using rects for the bulk of the filling, then single pixel fixups in the
   * following order:
   *
   * 00037
   * 00044
   * 11 44
   * 11555
   * 26555
   */
  
  screen_fill_rect(scr, x0, y0,     SIZE2D(w - 2, 2), a);
  screen_fill_rect(scr, x0, y0 + 2, SIZE2D(2, h - 3), a);
  screen_set_pixel(scr, x0, y1 - 1, a);
  screen_set_pixel(scr, x1 - 2, y0, a);

  screen_fill_rect(scr, x1 - 2, y0 + 1, SIZE2D(2, h - 1), b);
  screen_fill_rect(scr, x0 + 2, y1 - 2, SIZE2D(w - 2, 2), b);
  screen_set_pixel(scr, x0 + 1, y1 - 1, b);
  screen_set_pixel(scr, x1 - 1, y0, b);
}

void screen_draw_dashed_line(screen_t *scr,
                             int       x0,
                             int       y0,
                             int       x1,
                             int       y1,
                             int       on,
                             int       off,
                             colour_t  colour)
{
  box_t clip_box;
  box_t bounds;
  int   rx0, ry0, rx1, ry1;
  int   ox0, oy0;
  int   dx, dy;
  int   adx, ady;
  int   sx, sy;
  int   error, e2;
  int   period;
  int   phase;
  int   skipped;

  if (on <= 0)
    return;
  off    = MAX(off, 0);
  period = on + off;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  rx0 = x0;
  ry0 = y0;
  rx1 = x1;
  ry1 = y1;
  if (line_clip(&clip_box, &rx0, &ry0, &rx1, &ry1) == 0)
    return;

  ox0 = x0;
  oy0 = y0;

  screen_get_bounds(scr, &bounds);
  (void) line_clip(&bounds, &x0, &y0, &x1, &y1);

  /* line_clip may have moved the start point inward: keep the dash
   * pattern anchored to the original, unclipped start so it doesn't
   * shift as the line scrolls on/off screen. */
  skipped = MAX(abs(x0 - ox0), abs(y0 - oy0));

  dx  = x1 - x0;
  adx = abs(dx);
  sx  = SGN(dx);

  dy  = y1 - y0;
  ady = -abs(dy);
  sy  = SGN(dy);

  error = adx + ady;
  phase = skipped % period;

  for (;;)
  {
    if (phase < on)
      screen_set_pixel(scr, x0, y0, colour);
    if (++phase >= period)
      phase = 0;

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
   * to discard lines. screen_set_pixel() will be doing clipping too later. */
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

