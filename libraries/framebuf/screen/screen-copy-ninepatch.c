/* framebuf/screen/screen-copy-ninepatch.c -- 9-patch bitmap drawing */

#include <assert.h>

#include "base/utils.h"
#include "geom/box.h"
#include "geom/size.h"

#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

/* Build a bitmap_t view onto one cell (col,row) of a 3x3 grid within "src".
 * The view shares "src"'s rowbytes, so screen_copy_bitmap walks the parent
 * image correctly despite the narrower size. */
static void ninepatch_cell(bitmap_t       *cell,
                           const bitmap_t *src,
                           int             col,
                           int             row,
                           int             pw,
                           int             ph)
{
  int log2bpp;
  int bpp;

  /* Byte stride per pixel. Sub-byte formats have no meaningful cell offset. */
  log2bpp = pixelfmt_log2bpp(src->format);
  assert(log2bpp >= 3);
  bpp = 1 << (log2bpp - 3);

  *cell      = *src;
  cell->size = SIZE2D(pw, ph);
  cell->base = (unsigned char *) src->base
             + row * ph * src->rowbytes
             + col * pw * bpp;
}

/* Tile "cell" across "area" (a screen-space box), with the screen clip set to
 * the intersection of "area" and "saved". The draw origin starts at
 * (area->x0, area->y0) and steps by (stepx, stepy); a zero step means a single
 * row or column. Overhang past "area" is removed by the clip. */
static void tile_area(screen_t       *scr,
                      const box_t    *saved,
                      const box_t    *area,
                      const bitmap_t *cell,
                      int             stepx,
                      int             stepy)
{
  box_t clip;

  if (box_is_empty(saved))
    clip = *area;
  else if (box_intersection(saved, area, &clip))
    return; /* nothing visible */

  if (box_is_empty(&clip))
    return;

  scr->clip = clip;

  {
    int oy;
    int ox;

    for (oy = area->y0; oy < area->y1;
         oy += (stepy > 0) ? stepy : (area->y1 - oy))
    {
      for (ox = area->x0; ox < area->x1;
           ox += (stepx > 0) ? stepx : (area->x1 - ox))
        screen_copy_bitmap(scr, ox, oy, cell);
    }
  }
}

/* ----------------------------------------------------------------------- */

result_t screen_copy_ninepatch(screen_t       *scr,
                               const box_t    *dst,
                               const bitmap_t *src,
                               unsigned int    flags)
{
  box_t    saved;
  box_t    orig_clip;
  int      pw, ph;
  int      midx, midy;
  int      lx, rx, ty, by;
  bitmap_t cell;

  assert(src->size.w > 0 && src->size.w % 3 == 0);
  assert(src->size.h > 0 && src->size.h % 3 == 0);

  if (box_is_empty(dst))
    return result_OK;

  pw = src->size.w / 3;
  ph = src->size.h / 3;

  /* Corner column/row boundaries in the destination. When "dst" is narrower or
   * shorter than two patches the near and far corners would overlap, so each
   * boundary is clamped to the destination midpoint: the near corner gets the
   * near half, the far corner the far half, and the edge/centre runs between
   * them collapse to nothing. */
  midx = (dst->x0 + dst->x1) / 2;
  midy = (dst->y0 + dst->y1) / 2;
  lx = MIN(dst->x0 + pw, midx);
  rx = MAX(dst->x1 - pw, midx);
  ty = MIN(dst->y0 + ph, midy);
  by = MAX(dst->y1 - ph, midy);

  /* Fold "dst" into the saved clip once, so every tile_area call is bounded by
   * the destination rectangle as well as the caller's clip. An empty caller
   * clip means "no clipping", so in that case the bound is "dst" alone. */
  orig_clip = scr->clip;
  if (box_is_empty(&orig_clip))
    saved = *dst;
  else if (box_intersection(&orig_clip, dst, &saved))
    return result_OK; /* dst entirely outside the clip */

  /* Corners. */
  {
    box_t b;

    ninepatch_cell(&cell, src, 0, 0, pw, ph);
    b.x0 = dst->x0; b.y0 = dst->y0; b.x1 = lx; b.y1 = ty;
    tile_area(scr, &saved, &b, &cell, 0, 0);

    ninepatch_cell(&cell, src, 2, 0, pw, ph);
    b.x0 = rx; b.y0 = dst->y0; b.x1 = dst->x1; b.y1 = ty;
    tile_area(scr, &saved, &b, &cell, 0, 0);

    ninepatch_cell(&cell, src, 0, 2, pw, ph);
    b.x0 = dst->x0; b.y0 = by; b.x1 = lx; b.y1 = dst->y1;
    tile_area(scr, &saved, &b, &cell, 0, 0);

    ninepatch_cell(&cell, src, 2, 2, pw, ph);
    b.x0 = rx; b.y0 = by; b.x1 = dst->x1; b.y1 = dst->y1;
    tile_area(scr, &saved, &b, &cell, 0, 0);
  }

  /* Edges. */
  if (rx > lx)
  {
    box_t b;

    ninepatch_cell(&cell, src, 1, 0, pw, ph);
    b.x0 = lx; b.y0 = dst->y0; b.x1 = rx; b.y1 = ty;
    tile_area(scr, &saved, &b, &cell, pw, 0);

    ninepatch_cell(&cell, src, 1, 2, pw, ph);
    b.x0 = lx; b.y0 = by; b.x1 = rx; b.y1 = dst->y1;
    tile_area(scr, &saved, &b, &cell, pw, 0);
  }
  if (by > ty)
  {
    box_t b;

    ninepatch_cell(&cell, src, 0, 1, pw, ph);
    b.x0 = dst->x0; b.y0 = ty; b.x1 = lx; b.y1 = by;
    tile_area(scr, &saved, &b, &cell, 0, ph);

    ninepatch_cell(&cell, src, 2, 1, pw, ph);
    b.x0 = rx; b.y0 = ty; b.x1 = dst->x1; b.y1 = by;
    tile_area(scr, &saved, &b, &cell, 0, ph);
  }

  /* Centre. */
  if (rx > lx && by > ty && !(flags & screen_NINEPATCH_NO_CENTRE))
  {
    box_t b;

    ninepatch_cell(&cell, src, 1, 1, pw, ph);
    b.x0 = lx; b.y0 = ty; b.x1 = rx; b.y1 = by;
    tile_area(scr, &saved, &b, &cell, pw, ph);
  }

  scr->clip = orig_clip;

  return result_OK;
}
