/* screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "geom/box.h"

typedef struct screen screen_t;

// define screen origin etc.

struct screen
{
  bitmap_all_MEMBERS;
  box_t clip; /* rectangular clip region, specified in pixels */
};

void screen_init(screen_t  *scr,
                 int        width,
                 int        height,
                 pixelfmt_t fmt,
                 int        rowbytes,
                 colour_t  *palette,
                 box_t      clip,
                 void      *base);

void screen_for_bitmap(screen_t *scr, const bitmap_t *bm);

box_t screen_get_clip(const screen_t *scr);

#endif /* FRAMEBUF_SCREEN_H */
