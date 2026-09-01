/* framebuf/bitmap.h -- bitmap image type */

#ifndef FRAMEBUF_BITMAP_H
#define FRAMEBUF_BITMAP_H

#include "base/result.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/span.h"
#include "geom/size.h"

/** Common bitmap structure members (used for screens too). */
#define bitmap_COMMON_MEMBERS \
  size2d_t      size;          /**< Width and height of the bitmap in pixels. */ \
  pixelfmt_t    format;        /**< Pixel format of the bitmap. */               \
  int           rowbytes;      /**< Number of bytes per row of the bitmap. */    \
  colour_t     *palette;       /**< Palette of the bitmap, or NULL. */           \
  const span_t *span;          /**< Cached plotting functions for this format. */

/** All members required for a single bitmap. */
#define bitmap_ALL_MEMBERS \
  bitmap_COMMON_MEMBERS \
  void         *base; /**< Base address of the bitmap. */

/** A single bitmap. */
typedef struct bitmap
{
  bitmap_ALL_MEMBERS
}
bitmap_t;

/**
 * Initialise a previously allocated bitmap structure.
 *
 * \param[in] bm       Bitmap to initialise.
 * \param[in] size     Width and height of the bitmap in pixels.
 * \param[in] fmt      Pixel format of the bitmap.
 * \param[in] rowbytes Number of bytes per row of the bitmap.
 * \param[in] palette  Palette of the bitmap, or NULL.
 * \param[in] base     Base address of the bitmap.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bitmap_init(bitmap_t       *bm,
                     size2d_t        size,
                     pixelfmt_t      fmt,
                     int             rowbytes,
                     const colour_t *palette,
                     void           *base);

/**
 * Clear the given bitmap to the specified colour.
 *
 * \param[in] bm       Bitmap to clear.
 * \param[in] colour   Colour to clear the bitmap to.
 */
void bitmap_clear(bitmap_t *bm, colour_t colour);

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

/**
 * Load a PNG image into the given bitmap.
 *
 * \param[in] bm       Bitmap to load the image into.
 * \param[in] filename Filename of the image to load.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bitmap_load_png(bitmap_t *bm, const char *filename);

/**
 * Save the given bitmap as a PNG image.
 *
 * \param[in] bm       Bitmap to save.
 * \param[in] filename Filename to save the image to.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bitmap_save_png(const bitmap_t *bm, const char *filename);

/**
 * Convert the given bitmap into a different pixel format, allocating a new
 * bitmap structure for the result.
 *
 * \param[in] bm       Bitmap to convert.
 * \param[in] newfmt   New pixel format.
 * \param[out] newbm   Converted bitmap.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bitmap_convert(const bitmap_t *bm,
                        pixelfmt_t      newfmt,
                        bitmap_t      **newbm);

#endif /* FRAMEBUF_BITMAP_H */
