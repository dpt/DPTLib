/* bitmap.h */

#ifndef FRAMEBUF_BITMAP_H
#define FRAMEBUF_BITMAP_H

#include "framebuf/pixelfmt.h"

/** Common bitmap members. */
#define bitmap_MEMBERS      \
  int        width, height; \
  pixelfmt_t format;        \
  int        rowbytes

/** A bitmap. */
typedef struct bitmap bitmap_t;

struct bitmap
{
  bitmap_MEMBERS;
  void *base;
};

// it ought to be possible to provide instant flip_y by adjusting the base pointer and negating the rowbytes (which is why rowbytes is signed).
//void bitmap_flip_y(bitmap_t *bm)
//{
//  uint8_t *base;
//
//  base = bm->base;
//  base += bm->height * bm->rowbytes;
//
//  bm->rowbytes = -bm->rowbytes;
//
//  bm->base = base;
//}

#endif /* FRAMEBUF_BITMAP_H */
