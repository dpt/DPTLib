/* framebuf/octree.h -- octree colour quantiser */

/**
 * \file octree.h
 *
 * Builds a reduced palette for a deep image using the Gervautz-Purgathofer
 * octree. Pixels are inserted into an eight-level tree indexed by successive
 * bits of red, green and blue. Whenever the tree holds more than \ref
 * octree_MAX_COLOURS leaves, the deepest reducible node with the fewest
 * pixels is folded into a leaf. Memory therefore stays bounded, whatever the
 * number of distinct colours in the image. Once every pixel is in, the tree
 * is reduced to the requested size the same way, except that a fold which
 * would overshoot merges the node's two smallest children instead.
 */

#ifndef FRAMEBUF_OCTREE_H
#define FRAMEBUF_OCTREE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"

/* ----------------------------------------------------------------------- */

/** Smallest palette size \ref octree_make_palette accepts. */
#define octree_MIN_COLOURS (2)

/** Largest palette size \ref octree_make_palette accepts. */
#define octree_MAX_COLOURS (256)

/**
 * Compute an optimised palette for a deep bitmap.
 *
 * Alpha is ignored: every pixel counts as opaque. An image with no more than
 * `ncolours` distinct colours yields exactly those colours; otherwise the
 * palette holds exactly `ncolours` entries.
 *
 * \param[in]  src      Source bitmap, \ref pixelfmt_rgbx8888 or \ref
 *                      pixelfmt_rgba8888.
 * \param[in]  ncolours Maximum palette size, \ref octree_MIN_COLOURS to \ref
 *                      octree_MAX_COLOURS.
 * \param[out] palette  Receives up to `ncolours` opaque entries.
 * \param[out] nfound   Receives the number of entries written.
 * \return \ref result_OK on success, \ref result_BAD_ARG for an unsupported
 *         format or palette size, \ref result_OOM on allocation failure.
 */
result_t octree_make_palette(const bitmap_t *src,
                             int             ncolours,
                             colour_t       *palette,
                             int            *nfound);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_OCTREE_H */
