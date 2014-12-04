/* screen.h -- screen type */

#ifndef FRAMEBUF_SCREEN_H
#define FRAMEBUF_SCREEN_H

#include "framebuf/bitmap.h"
#include "geom/box.h"

typedef struct screen screen_t;

// define screen origin etc.

struct screen
{
  bitmap_MEMBERS;
  box_t  clip; /* rectangular clip region, specified in pixels */
  void  *base;
};

#endif /* FRAMEBUF_SCREEN_H */
