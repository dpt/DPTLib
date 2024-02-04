/* bitmap.h -- bitmap image type */

#ifndef FRAMEBUF_BITMAP_H
#define FRAMEBUF_BITMAP_H

#include "base/result.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/span.h"

/** Common bitmap members. */
#define bitmap_format_MEMBERS  \
  int           width, height; \
  pixelfmt_t    format;        \
  int           rowbytes;      \
  colour_t     *palette;       \
  const span_t *span;

/** Common bitmap members. */
#define bitmap_all_MEMBERS     \
  int           width, height; \
  pixelfmt_t    format;        \
  int           rowbytes;      \
  colour_t     *palette;       \
  const span_t *span;          \
  void         *base

/** A bitmap. */
typedef struct bitmap bitmap_t;

struct bitmap
{
  bitmap_all_MEMBERS;
};

result_t bitmap_init(bitmap_t       *bm,
                     int             width,
                     int             height,
                     pixelfmt_t      fmt,
                     int             rowbytes,
                     const colour_t *palette,
                     void           *base);

void bitmap_clear(bitmap_t *bm, colour_t c);

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

result_t bitmap_load_png(bitmap_t *bm, const char *filename);
result_t bitmap_save_png(const bitmap_t *bm, const char *filename);

result_t bitmap_convert(const bitmap_t *bm, pixelfmt_t newfmt, bitmap_t **newbm);

#endif /* FRAMEBUF_BITMAP_H */
