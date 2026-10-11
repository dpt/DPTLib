/* framebuf/screen/screen-copy-tiled.c -- tiled bitmap drawing */

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

/* Left (or top) edge of the tile containing "v", for tiles "step" wide that
 * have one edge at "origin". Correct for v below origin, where % goes
 * negative. */
static int tile_start(int v, int origin, int step)
{
  int d;

  d = (v - origin) % step;
  if (d < 0)
    d += step;

  return v - d;
}

/* ----------------------------------------------------------------------- */

result_t screen_copy_bitmap_tiled(screen_t       *scr,
                                  const box_t    *dst,
                                  const bitmap_t *src,
                                  point_t         origin)
{
  result_t rc;
  box_t    orig_clip;
  box_t    clip;
  int      x0;
  int      x;
  int      y;

  if (src->size.w <= 0 || src->size.h <= 0)
    return result_BAD_ARG;

  if (box_is_empty(dst))
    return result_OK;

  /* An empty screen clip means "no clipping", so "dst" alone bounds the
   * drawing. */
  orig_clip = scr->clip;
  if (box_is_empty(&orig_clip))
    clip = *dst;
  else if (box_intersection(&orig_clip, dst, &clip))
    return result_OK; /* dst entirely outside the clip */

  if (box_is_empty(&clip))
    return result_OK;

  scr->clip = clip;

  rc = result_OK;
  x0 = tile_start(clip.x0, origin.x, src->size.w);
  y  = tile_start(clip.y0, origin.y, src->size.h);
  for (; y < clip.y1 && rc == result_OK; y += src->size.h)
    for (x = x0; x < clip.x1 && rc == result_OK; x += src->size.w)
      rc = screen_copy_bitmap(scr, x, y, src);

  scr->clip = orig_clip;

  return rc;
}
