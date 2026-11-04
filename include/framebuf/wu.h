/* framebuf/wu.h -- Wu's colour quantiser */

/**
 * \file wu.h
 *
 * Builds a reduced palette for a deep image using a variant of Wu's greedy
 * orthogonal bipartition (Xiaolin Wu, 1991). Pixels are counted into a
 * histogram of 32 bins per channel, which is summed into cumulative tables
 * so that any box of bins can be totalled in constant time. Starting from
 * one box spanning all of RGB, the box whose best axis-aligned cut most
 * reduces the total squared error is split until the palette is full. Each
 * palette entry is the mean of the pixels in its box.
 *
 * Memory and the work done after the histogram depend upon the palette size,
 * not upon the image's size. No pixel costs a division.
 */

#ifndef FRAMEBUF_WU_H
#define FRAMEBUF_WU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"

/* ----------------------------------------------------------------------- */

/** Smallest palette size \ref wu_make_palette accepts. */
#define wu_MIN_COLOURS (2)

/** Largest palette size \ref wu_make_palette accepts. */
#define wu_MAX_COLOURS (256)

/**
 * Compute an optimised palette for a deep bitmap.
 *
 * Alpha is ignored: every pixel counts as opaque. Colours sharing a
 * histogram bin (the same top five bits of each channel) always merge, so
 * the palette holds `ncolours` entries or one per occupied bin, whichever is
 * fewer. It can therefore hold fewer entries than the image has colours, and
 * an entry is exactly an image's colour only if it is alone in its bin.
 *
 * \param[in]  src      Source bitmap, \ref pixelfmt_rgbx8888 or \ref
 *                      pixelfmt_rgba8888.
 * \param[in]  ncolours Maximum palette size, \ref wu_MIN_COLOURS to \ref
 *                      wu_MAX_COLOURS.
 * \param[out] palette  Receives up to `ncolours` opaque entries.
 * \param[out] nfound   Receives the number of entries written.
 * \return \ref result_OK on success, \ref result_BAD_ARG for an unsupported
 *         format or palette size, \ref result_TOO_BIG for an image with too
 *         many pixels, \ref result_OOM on allocation failure.
 */
result_t wu_make_palette(const bitmap_t *src,
                         int             ncolours,
                         colour_t       *palette,
                         int            *nfound);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_WU_H */
