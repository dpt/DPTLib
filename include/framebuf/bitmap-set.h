/* bitmap-set.h -- a set of bitmap images */

#ifndef FRAMEBUF_BITMAP_SET_H
#define FRAMEBUF_BITMAP_SET_H

#include "framebuf/bitmap.h"

/**
 * A set of identical bitmaps.
 */
typedef struct bitmap_set
{
  bitmap_COMMON_MEMBERS;
  void **bases;  /**< Array of bitmap base pointers. */
  int    nbases; /**< Number of bitmap base pointers. */
}
bitmap_set_t;

#endif /* FRAMEBUF_BITMAP_SET_H */
