/* bitmap-set.h -- a set of bitmap images */

#ifndef FRAMEBUF_BITMAP_SET_H
#define FRAMEBUF_BITMAP_SET_H

#include "framebuf/bitmap.h"

/** A set of identical bitmaps. */
typedef struct bitmap_set bitmap_set_t;

struct bitmap_set
{
  bitmap_MEMBERS;
  void **bases;  /* an array of bitmap base pointers */
  int    nbases;
};

#endif /* FRAMEBUF_BITMAP_SET_H */
