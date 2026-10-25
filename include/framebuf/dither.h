/* framebuf/dither.h -- error-diffusion dithering to a palette */

/**
 * \file dither.h
 *
 * Reduces a deep image to a paletted one, spreading each pixel's
 * quantisation error over its unvisited neighbours using a standard
 * error-diffusion kernel. Errors are kept per channel in integer arithmetic
 * and diffused in sRGB or, optionally, in linear light. Nearest-colour
 * lookups go through \ref pixelmap_get, so they are always made in sRGB.
 */

#ifndef FRAMEBUF_DITHER_H
#define FRAMEBUF_DITHER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"

/* ----------------------------------------------------------------------- */

/** Error-diffusion kernels. */
typedef enum dither_method
{
  dither_NONE,               /**< Nearest colour only, no diffusion. */
  dither_FLOYD_STEINBERG,
  dither_JARVIS_JUDICE_NINKE,
  dither_STUCKI,
  dither_ATKINSON,           /**< Diffuses only 6/8 of the error. */
  dither_BURKES,
  dither_SIERRA,
  dither_SIERRA_TWO_ROW,
  dither_SIERRA_LITE,        /**< Cheapest kernel: three neighbours. */
  dither__LIMIT
}
dither_method_t;

/** Options for \ref dither_bitmap. */
typedef struct dither_options
{
  dither_method_t method;
  const colour_t *palette;    /**< Target palette. */
  int             ncolours;   /**< Entries in `palette`, 1 to 256. */
  int             strength;   /**< Percentage of error diffused, 0 to 100. */
  int             serpentine; /**< Non-zero: alternate row direction. */
  int             linear;     /**< Non-zero: diffuse in linear light. */
}
dither_options_t;

/**
 * Dither a deep bitmap to a newly allocated paletted bitmap.
 *
 * Alpha is ignored. The output takes the smallest paletted format which
 * holds `opts->ncolours` entries: \ref pixelfmt_p1, \ref pixelfmt_p2, \ref
 * pixelfmt_p4 or \ref pixelfmt_p8. Its palette is `opts->palette`, padded
 * with black to the format's full entry count. The caller frees the result's
 * `base`, its `palette` and the bitmap itself.
 *
 * Serpentine scanning runs odd rows right to left, mirroring the kernel. It
 * avoids the diagonal artefacts that raster order produces.
 *
 * Linear diffusion decodes each channel from sRGB before the error is added
 * and measured, so the dithered result keeps the source's physical
 * brightness. Diffusing in sRGB instead makes sparse palettes come out too
 * bright in the midtones.
 *
 * \param[in]  src  Source bitmap, \ref pixelfmt_rgbx8888 or \ref
 *                  pixelfmt_rgba8888.
 * \param[in]  opts Dithering options.
 * \param[out] dst  Receives the dithered bitmap.
 * \return \ref result_OK on success, \ref result_BAD_ARG for an unsupported
 *         format or option, \ref result_OOM on allocation failure.
 */
result_t dither_bitmap(const bitmap_t         *src,
                       const dither_options_t *opts,
                       bitmap_t              **dst);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_DITHER_H */
