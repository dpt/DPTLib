/* screen.c */

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
