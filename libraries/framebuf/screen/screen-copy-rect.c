/* framebuf/screen/screen-copy-rect.c -- same-screen rectangle copy */

#include <string.h>

#include "base/utils.h"

#include "framebuf/screen.h"

/* Sub-byte paletted screens (1/2/4bpp) pack several pixels per byte, so a
 * moving window's source and destination pixels don't line up on byte
 * boundaries in general. Each row is copied a destination byte at a time:
 * the byte's pixels come from a shifted two-byte window of the source row,
 * so a big window costs a few operations per byte rather than per pixel.
 * Rows (and bytes within a row) are walked in the direction the shift
 * dictates, so every source byte is read before anything that could
 * overwrite it is written. "bpp_bits" is 1, 2 or 4.
 *
 * The within-byte pixel order must match the rest of the screen code, which
 * is not uniform: p1 and p2 are MSB-first (bit 7 / bits 7..6 are the leftmost
 * pixel -- see screen_set_pixel_p1/p2 and screen_copy_bitmap_p1/p2) while p4
 * is LSB-first (see screen_set_pixel_p4). "msb_first" carries that. */

/* One destination byte of a row: dest pixel x takes source pixel x - "dx".
 * Only the pixels in [x0, x1) are written; the rest of the byte is kept. */
static void screen_copy_rect_packed_byte(unsigned char       *drow,
                                         const unsigned char *srow,
                                         int                  rowbytes,
                                         int                  byte,
                                         int                  x0,
                                         int                  x1,
                                         int                  dx,
                                         int                  bpp_bits)
{
  int           ppb, xshift, xmask, msb_first;
  int           s0, sb, phase, lo, hi, i, shift;
  unsigned int  w, b0, b1;
  unsigned char mask, v;

  ppb       = 8 / bpp_bits;
  xshift    = (bpp_bits == 1) ? 3 : (bpp_bits == 2) ? 2 : 1;
  xmask     = ppb - 1;
  msb_first = (bpp_bits != 4);

  /* the source pixel under this byte's first pixel, and the byte holding it */
  s0    = byte * ppb - dx;
  sb    = (s0 >= 0) ? (s0 >> xshift) : -((-s0 + xmask) >> xshift);
  phase = s0 - sb * ppb;

  b0 = (sb >= 0 && sb < rowbytes) ? srow[sb] : 0;
  b1 = (phase != 0 && sb + 1 >= 0 && sb + 1 < rowbytes) ? srow[sb + 1] : 0;

  if (msb_first)
  {
    w = (b0 << 8) | b1;
    v = (unsigned char) (w >> (8 - phase * bpp_bits));
  }
  else
  {
    w = b0 | (b1 << 8);
    v = (unsigned char) (w >> (phase * bpp_bits));
  }

  lo = MAX(x0, byte * ppb);
  hi = MIN(x1, (byte + 1) * ppb);
  if (hi - lo == ppb)
  {
    drow[byte] = v;
    return;
  }

  mask = 0;
  for (i = lo; i < hi; i++)
  {
    shift = (i & xmask) * bpp_bits;
    if (msb_first)
      shift = 8 - bpp_bits - shift;
    mask = (unsigned char) (mask | (((1u << bpp_bits) - 1) << shift));
  }

  drow[byte] = (unsigned char) ((drow[byte] & ~mask) | (v & mask));
}

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
  int            xshift, ppb;
  int            first, last, step, byte;
  int            row_first, row_last, row_step;
  int            row;

  base     = scr->base;
  rowbytes = scr->rowbytes;

  xshift = (bpp_bits == 1) ? 3 : (bpp_bits == 2) ? 2 : 1;
  ppb    = 8 / bpp_bits;

  first = d->x0 >> xshift;
  last  = (d->x0 + width - 1) >> xshift;

  /* a move right reads from lower bytes than it writes, so go high to low */
  if (dx > 0) { byte = last;  step = -1; }
  else        { byte = first; step =  1; }

  if (dy > 0) { row_first = height - 1; row_last = -1;     row_step = -1; }
  else        { row_first = 0;          row_last = height; row_step =  1; }

  for (row = row_first; row != row_last; row += row_step)
  {
    unsigned char       *drow;
    const unsigned char *srow;
    int                  b, shift_bytes;

    drow = base + (size_t) (d->y0 + row) * rowbytes;
    srow = base + (size_t) (s->y0 + row) * rowbytes;

    if (dx != 0 && dx % ppb == 0 && last - first >= 2)
    {
      /* byte-aligned shift: every whole byte in the middle is a straight
       * copy; only the two partial end bytes need the masked path. Order the
       * ends so neither reads source bytes the middle copy has overwritten */
      shift_bytes = dx / ppb;

      if (dx > 0)
        screen_copy_rect_packed_byte(drow, srow, rowbytes, last, d->x0,
                                     d->x0 + width, dx, bpp_bits);
      else
        screen_copy_rect_packed_byte(drow, srow, rowbytes, first, d->x0,
                                     d->x0 + width, dx, bpp_bits);

      memmove(drow + first + 1, srow + first + 1 - shift_bytes,
              (size_t) (last - first - 1));

      if (dx > 0)
        screen_copy_rect_packed_byte(drow, srow, rowbytes, first, d->x0,
                                     d->x0 + width, dx, bpp_bits);
      else
        screen_copy_rect_packed_byte(drow, srow, rowbytes, last, d->x0,
                                     d->x0 + width, dx, bpp_bits);
      continue;
    }

    for (b = byte; b >= first && b <= last; b += step)
      screen_copy_rect_packed_byte(drow, srow, rowbytes, b, d->x0,
                                   d->x0 + width, dx, bpp_bits);
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
