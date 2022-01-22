/* screen.c */

#include <assert.h>
#include <string.h>
#include <stddef.h>

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

box_t screen_get_clip(const screen_t *scr)
{
  box_t cur;

  cur.x0 = 0;
  cur.y0 = 0;
  cur.x1 = scr->width;
  cur.y1 = scr->height;

  if (box_is_empty(&scr->clip))
    return cur;

  (void) box_intersection(&cur, &scr->clip, &cur);
  return cur;
}
