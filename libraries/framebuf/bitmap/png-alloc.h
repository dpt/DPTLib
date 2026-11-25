/* framebuf/bitmap/png-alloc.h -- route libpng's memory through an alloc_t */

#ifndef FRAMEBUF_BITMAP_PNG_ALLOC_H
#define FRAMEBUF_BITMAP_PNG_ALLOC_H

#include <png.h>

#include "utils/alloc.h"

/* Pass these to png_create_{read,write}_struct_2 with the alloc_t as the
 * mem_ptr. */

static inline png_voidp png_alloc_malloc(png_structp      png_ptr,
                                         png_alloc_size_t size)
{
  const alloc_t *alloc = png_get_mem_ptr(png_ptr);

  return alloc->malloc(size);
}

static inline void png_alloc_free(png_structp png_ptr, png_voidp ptr)
{
  const alloc_t *alloc = png_get_mem_ptr(png_ptr);

  alloc->free(ptr);
}

#endif /* FRAMEBUF_BITMAP_PNG_ALLOC_H */

/* vim: set ts=8 sts=2 sw=2 et: */
