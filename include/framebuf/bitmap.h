/* framebuf/bitmap.h -- bitmap image type */

#ifndef FRAMEBUF_BITMAP_H
#define FRAMEBUF_BITMAP_H

#include <stdint.h>

#include "base/result.h"
#include "framebuf/colour.h"
#include "framebuf/pattern.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/span.h"
#include "geom/box.h"
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
 * Replace a bitmap's palette, reusing its existing palette buffer when one
 * is already allocated and it is large enough. A no-op returning \ref
 * result_OK for formats with no palette (log2bpp > 3).
 *
 * \param[in] bm      Bitmap to repalette.
 * \param[in] palette New palette, copied in, or NULL to drop the bitmap's
 *                    palette entirely.
 * \return \ref result_OK on success, \ref result_OOM if a palette buffer
 *         could not be allocated (the bitmap keeps its old palette).
 */
result_t bitmap_set_palette(bitmap_t *bm, const colour_t *palette);

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
 * Fill a rectangle of the bitmap with a repeating 8x8 pattern.
 *
 * A plain pattern paints every pixel in the area, set bits taking
 * `pattern->fg` and clear bits `pattern->bg`. A stencil pattern (one with
 * `pattern_FLAG_STENCIL`) paints only the set-bit pixels, leaving the rest
 * untouched. The tile is phased against `pattern->origin`, so abutting fills
 * with the same origin line up.
 *
 * Supported for 8bpp and 32bpp formats only; other formats return \ref
 * result_NOT_SUPPORTED.
 *
 * \param[in] bm      Bitmap to fill.
 * \param[in] area    Rectangle to fill, clipped to the bitmap bounds. NULL
 *                    fills the whole bitmap.
 * \param[in] pattern Pattern to fill with.
 * \return \ref result_OK on success, \ref result_NOT_SUPPORTED for an
 *         unsupported pixel format.
 */
result_t bitmap_fill_pattern(bitmap_t        *bm,
                             const box_t     *area,
                             const pattern_t *pattern);

/**
 * Load a PNG image into the given bitmap.
 *
 * A palette-type PNG is kept as \ref pixelfmt_p8 with its PLTE (and any
 * tRNS) copied into `bm->palette`; RGB and greyscale PNGs are expanded to a
 * 32bpp `rgbx8888` / `rgba8888` bitmap with no palette.
 *
 * \param[in] bm       Bitmap to load the image into.
 * \param[in] filename Filename of the image to load.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bitmap_load_png(bitmap_t *bm, const char *filename);

/**
 * Save the given bitmap as a PNG image.
 *
 * Supported formats: `bgrx8888` (written as RGB), `bgra8888` (RGBA), and
 * \ref pixelfmt_p8 (written as a palette-type PNG, its `bm->palette`
 * becoming the PLTE, plus a tRNS chunk if any palette entry is non-opaque).
 * Other formats return \ref result_NOT_SUPPORTED, as does a \ref pixelfmt_p8
 * bitmap with no palette.
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

/** Non-zero if `bm` holds an RLE-compressed (read-only) pixel store. */
#define bitmap_is_compressed(bm) pixelfmt_is_rle((bm)->format)

/**
 * Compress a bitmap's pixels in place into a read-only run-length-encoded
 * blob, replacing `bm->base`. `bm->size`, `bm->palette` and `bm->span` stay
 * valid for the decoded format; `bm->format` gains \ref pixelfmt_RLE_FLAG
 * and `bm->rowbytes` becomes 0. Idempotent (a second call returns \ref
 * result_OK).
 *
 * Suitable for sparse UI art: long single-colour runs and, for the alpha
 * formats, fully-transparent regions collapse to a few bytes, saving RAM and
 * blit-time memory bandwidth. Only \ref screen_copy_bitmap can consume the
 * result; other bitmap operations decline with \ref result_NOT_SUPPORTED.
 *
 * Supported source formats: \ref pixelfmt_y8, the 32bpp `*8888` formats
 * (`bgrx`/`rgbx`/`xbgr`/`xrgb`) and the 32bpp alpha formats
 * (`bgra`/`rgba`/`abgr`/`argb`). Transparent-run compression applies to
 * `bgra8888`/`rgba8888` only.
 *
 * \param[in] bm Bitmap to compress in place.
 * \return \ref result_OK on success, \ref result_NOT_SUPPORTED for an
 *         unsupported format, \ref result_OOM on allocation failure.
 */
result_t bitmap_compress(bitmap_t *bm);

/**
 * Reverse \ref bitmap_compress, restoring a raw `rowbytes`-strided pixel
 * buffer byte-for-byte. `bm->format` loses \ref pixelfmt_RLE_FLAG and
 * `bm->rowbytes` is restored. A no-op returning \ref result_OK if `bm` is
 * not compressed.
 *
 * \param[in] bm Bitmap to decompress in place.
 * \return \ref result_OK on success, \ref result_OOM on allocation failure.
 */
result_t bitmap_decompress(bitmap_t *bm);

#endif /* FRAMEBUF_BITMAP_H */
