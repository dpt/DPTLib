/* framebuf/screen/screen-copy-rect.c -- same-screen rectangle copy */

#include <string.h>

#include "framebuf/screen.h"

/* Sub-byte paletted screens (1/2/4bpp) pack several pixels per byte, so a
 * moving window's source and destination pixels don't line up on byte
 * boundaries in general. Copied in place, pixel by pixel, masking each one
 * out and rolling it into place: safe under self-overlap by walking rows in
 * the direction "dy" dictates (as the byte path already does) and, since a
 * pixel's byte-mates are its immediate horizontal neighbours, walking columns
 * within each row by the same rule applied to "dx" -- the standard two-axis
 * blit-direction trick, so every pixel is read before anything that could
 * overwrite it is written. "bpp_bits" is 1, 2 or 4.
 *
 * The within-byte pixel order must match the rest of the screen code, which
 * is not uniform: p1 and p2 are MSB-first (bit 7 / bits 7..6 are the leftmost
 * pixel -- see screen_set_pixel_p1/p2 and screen_copy_bitmap_p1/p2) while p4
 * is LSB-first (see screen_set_pixel_p4). "msb_first" carries that. */
static result_t screen_copy_rect_packed(screen_t    *scr,
                                        const box_t *s,
                                        const box_t *d,
                                        int          width,
                                        int          height,
                                        int          dx,
                                        int          dy,
                                        int          bpp_bits)
{
  unsigned char *base;
  int            rowbytes;
  int            ppb, xshift, xmask, msb_first;
  unsigned char  pmask;
  int            row_first, row_last, row_step;
  int            col_first, col_last, col_step;
  int            row, col;

  base     = scr->base;
  rowbytes = scr->rowbytes;

  ppb       = 8 / bpp_bits;        /* pixels per byte: 8, 4 or 2 */
  xshift    = (bpp_bits == 1) ? 3 : (bpp_bits == 2) ? 2 : 1;
  xmask     = ppb - 1;
  pmask     = (unsigned char) ((1u << bpp_bits) - 1);
  msb_first = (bpp_bits != 4);

  if (dy > 0) { row_first = height - 1; row_last = -1;     row_step = -1; }
  else        { row_first = 0;          row_last = height; row_step =  1; }

  if (dx > 0) { col_first = width - 1;  col_last = -1;      col_step = -1; }
  else        { col_first = 0;          col_last = width;   col_step =  1; }

  for (row = row_first; row != row_last; row += row_step)
  {
    for (col = col_first; col != col_last; col += col_step)
    {
      int                  sx, sy, dxp, dyp;
      const unsigned char *scrp_s;
      unsigned char        *scrp_d;
      int                   shift_s, shift_d;
      unsigned char         pix;

      sx  = s->x0 + col; sy  = s->y0 + row;
      dxp = d->x0 + col; dyp = d->y0 + row;

      scrp_s  = base + (size_t) sy * rowbytes + (sx >> xshift);
      shift_s = (sx & xmask) * bpp_bits;
      if (msb_first)
        shift_s = 8 - bpp_bits - shift_s;
      pix     = (unsigned char) ((*scrp_s >> shift_s) & pmask);

      scrp_d  = base + (size_t) dyp * rowbytes + (dxp >> xshift);
      shift_d = (dxp & xmask) * bpp_bits;
      if (msb_first)
        shift_d = 8 - bpp_bits - shift_d;
      *scrp_d = (unsigned char) ((*scrp_d & ~(pmask << shift_d))
                                 | (pix << shift_d));
    }
  }

  return result_OK;
}

/* Whole-byte formats (8/16/32bpp): a row of "width" pixels is "width * bpp"
 * contiguous bytes, so each row moves in one memmove. Rows can alias across
 * each other (never within a row) when the vertical shift is smaller than the
 * copied height, so walk in the direction that always reads a row before it
 * is overwritten. */
static result_t screen_copy_rect_bytes(screen_t    *scr,
                                       const box_t *s,
                                       const box_t *d,
                                       int          width,
                                       int          height,
                                       int          dy,
                                       int          bpp)
{
  unsigned char *base;
  int            rowbytes;
  int            row;

  base     = scr->base;
  rowbytes = scr->rowbytes;

  if (dy > 0)
  {
    for (row = height - 1; row >= 0; row--)
      memmove(base + (size_t) (d->y0 + row) * rowbytes + (size_t) d->x0 * bpp,
              base + (size_t) (s->y0 + row) * rowbytes + (size_t) s->x0 * bpp,
              (size_t) width * bpp);
  }
  else
  {
    for (row = 0; row < height; row++)
      memmove(base + (size_t) (d->y0 + row) * rowbytes + (size_t) d->x0 * bpp,
              base + (size_t) (s->y0 + row) * rowbytes + (size_t) s->x0 * bpp,
              (size_t) width * bpp);
  }

  return result_OK;
}

result_t screen_copy_rect(screen_t    *scr,
                          const box_t *src,
                          point_t      dst,
                          box_t       *copied_dst)
{
  box_t clip_box, s, d, d_clipped;
  int   dx, dy, width, height, bpp;

  if (screen_get_clip(scr, &clip_box))
    return result_NOT_SUPPORTED; /* invalid clipped screen */

  dx = dst.x - src->x0;
  dy = dst.y - src->y0;

  if (box_intersection(&clip_box, src, &s))
    return result_NOT_SUPPORTED; /* source entirely off-screen */

  box_translated(&s, dx, dy, &d);
  if (box_intersection(&clip_box, &d, &d_clipped))
    return result_NOT_SUPPORTED; /* destination entirely off-screen */

  /* keep only the part of "s" whose translated position also survived
   * clipping, so source and destination stay the same size */
  s.x0 += d_clipped.x0 - d.x0;
  s.x1 += d_clipped.x1 - d.x1;
  s.y0 += d_clipped.y0 - d.y0;
  s.y1 += d_clipped.y1 - d.y1;

  if (box_is_empty(&s))
    return result_NOT_SUPPORTED;

  width  = s.x1 - s.x0;
  height = s.y1 - s.y0;

  /* "src"/dst describe the ideal, unclipped move: when either end
   * falls partly off-screen, "d_clipped" (the part actually copied) can be
   * smaller than the caller's intended destination, e.g. a window dragged
   * back on-screen from off-screen has no source pixels for the part that
   * was never drawn. Callers must invalidate the rest of their intended
   * destination themselves rather than assume the whole of it is now
   * correct. */
  if (copied_dst != NULL)
    *copied_dst = d_clipped;

  switch (pixelfmt_log2bpp(scr->format))
  {
  case 0: return screen_copy_rect_packed(scr, &s, &d_clipped,
                                         width, height, dx, dy, 1);
  case 1: return screen_copy_rect_packed(scr, &s, &d_clipped,
                                         width, height, dx, dy, 2);
  case 2: return screen_copy_rect_packed(scr, &s, &d_clipped,
                                         width, height, dx, dy, 4);

  case 3: bpp = 1; break;
  case 4: bpp = 2; break;
  case 5: bpp = 4; break;

  default:
    /* ponytail: unknown/future pixel format; callers must fall back to a
     * normal invalidate/redraw there */
    return result_NOT_SUPPORTED;
  }

  return screen_copy_rect_bytes(scr, &s, &d_clipped, width, height, dy, bpp);
}
