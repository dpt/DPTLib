/* framebuf/dither.h -- error-diffusion and ordered dithering to a palette */

/**
 * \file dither.h
 *
 * Reduces a deep image to a paletted one. Error-diffusion methods spread
 * each pixel's quantisation error over its unvisited neighbours using a
 * standard kernel. Errors are kept per channel in integer arithmetic and
 * diffused in sRGB or, optionally, in linear light. Ordered methods instead
 * nudge each pixel by a fixed threshold map before the lookup, again
 * optionally in linear light. Nearest-colour lookups go through \ref
 * pixelmap_get, so they are always made in sRGB.
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

/**
 * Dithering methods: nearest colour, then the threshold methods, then the
 * error-diffusion methods, each group oldest first.
 */
typedef enum dither_method
{
  dither_NONE,               /**< Nearest colour only, no diffusion. */

  dither_CLUSTERED,          /**< Ordered, 8x8 clustered-dot halftone. */
  dither_RANDOM,             /**< Threshold from white noise. */
  dither_BAYER_2,            /**< Ordered, 2x2 Bayer matrix. */
  dither_BAYER_4,            /**< Ordered, 4x4 Bayer matrix. */
  dither_BAYER_8,            /**< Ordered, 8x8 Bayer matrix. */
  dither_BAYER_16,           /**< Ordered, 16x16 Bayer matrix. */
  dither_BLUE_NOISE,         /**< Ordered, 64x64 blue-noise map. */
  dither_KNOLL,              /**< Knoll pattern dither, Bayer-ordered. */
  dither_YLILUOMA,           /**< Yliluoma's mixing-plan dither. */

  dither_FLOYD_STEINBERG,
  dither_JARVIS_JUDICE_NINKE,
  dither_STUCKI,
  dither_ATKINSON,           /**< Diffuses only 6/8 of the error. */
  dither_BURKES,
  dither_SIERRA,
  dither_SIERRA_TWO_ROW,
  dither_SIERRA_LITE,        /**< Cheapest kernel: three neighbours. */
  dither_FAN,                /**< Floyd-Steinberg reaching further left. */
  dither_SHIAU_FAN,          /**< Four cells, fewer worms than Fan. */
  dither_SHIAU_FAN_2,        /**< Five cells, reaching three left. */
  dither_RIEMERSMA,          /**< Diffuses along a Hilbert curve. */
  dither_OSTROMOUKHOV,       /**< Coefficients vary with level. */
  dither_ZHOU_FANG,          /**< Ostromoukhov plus threshold noise. */
  dither__LIMIT
}
dither_method_t;

/** Options for \ref dither_bitmap. */
typedef struct dither_options
{
  dither_method_t method;
  const colour_t *palette;    /**< Target palette. */
  int             ncolours;   /**< Entries in `palette`, 1 to 256. */
  int             strength;   /**< Percentage of error diffused, of the
                               *   ordered spread or of Knoll's carried
                               *   error, 0 to 100. */
  int             serpentine; /**< Non-zero: alternate row direction. */
  int             linear;     /**< Non-zero: dither in linear light. */
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
 * The error a pixel carries is limited to two of the palette's typical gaps
 * per channel, scaled down as a whole so its hue is kept. Error the palette
 * cannot express, such as a colour's chroma against a grey palette, would
 * otherwise build up until channels clip, darkening or lightening the
 * result.
 *
 * Ordered methods add a bias of up to half the palette's typical channel gap
 * (\ref pixelmap_t's `step`) either way, chosen per pixel by the threshold
 * map. They have no error to carry, so they ignore `serpentine`. With
 * `linear` the bias is added in linear light, rescaled to the linear width
 * of the gap around each channel, so the threshold splits pixels between
 * palette entries in proportion to their physical brightness.
 *
 * \ref dither_KNOLL is Thomas Knoll's pattern dither. Per pixel it picks 16
 * candidate colours whose mix approximates the source, sorts them by luma
 * and takes the one the Bayer threshold selects. It costs 16 lookups a pixel
 * but suits fixed, uneven palettes better than a plain ordered bias. Like
 * the ordered methods it ignores `serpentine`; with `linear` its candidates'
 * error is carried in linear light.
 *
 * \ref dither_YLILUOMA is Joel Yliluoma's arbitrary-palette positional
 * dither, his algorithm 2. Per pixel it builds a plan of 16 palette entries,
 * each step adding the entry, in a power-of-two quantity, whose mix with
 * those so far is psychovisually closest to the source. The plan is sorted
 * by luma and the Bayer threshold picks from it. Every step tries the whole
 * palette, so plans are cached per colour cut to 5:6:5 bits. Even so it is
 * far slower than Knoll on large palettes, but its mixes are closer.
 * `strength` scales how far the threshold strays from the plan's middle
 * entry. It ignores `serpentine`; with `linear` its plans mix in linear
 * light.
 *
 * \ref dither_RIEMERSMA walks the image along a Hilbert curve and adds to
 * each pixel a weighted sum of the last 16 errors, the newest weighted most.
 * The curve has no preferred direction, so the result shows none of a
 * kernel's directional artefacts. It honours `linear` and ignores
 * `serpentine`.
 *
 * \ref dither_OSTROMOUKHOV diffuses to the same three neighbours as
 * Floyd-Steinberg's right, down-left and down, but splits each channel's
 * error by a table indexed by that channel's source level. The table is
 * tuned for blue-noise output across the whole tonal range. It is meant to
 * be run serpentine.
 *
 * \ref dither_ZHOU_FANG adds Zhou and Fang's threshold modulation to that
 * diffusion: each lookup sees the pixel nudged by white noise whose spread
 * depends on the source level, while the error carried on is measured from
 * the un-nudged pixel. The noise breaks up the worms and regular patterns
 * left in the midtones. It uses Zhou and Fang's own coefficients and is
 * seeded the same every call, so it is repeatable. `strength` scales the
 * noise as well as the error.
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
