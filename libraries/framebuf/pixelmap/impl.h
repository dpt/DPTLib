/* framebuf/pixelmap/impl.h -- pixelmap internals */

#ifndef PIXELMAP_IMPL_H
#define PIXELMAP_IMPL_H

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/pixelmap.h"

/* ----------------------------------------------------------------------- */

/**
 * Fill in the channel layout of `out` (rbits/gbits/bbits,
 * rshift/gshift/bshift, destfmt, dest_log2bpp, nentries) for the given
 * format pair, from the built-in policy table.
 *
 * The source format must be a 32bpp `*8888` format; its channels are
 * extracted with an 8-bit right shift (\ref pixelmap_t
 * rshift/gshift/bshift).
 *
 * \param[in]  srcfmt  Source pixel format.
 * \param[in]  destfmt Destination format.
 * \param[out] out     Partly-filled pixelmap (entries not touched).
 * \return 0 on success, -1 if the pair is unsupported.
 */
int pixelmap__layout_for(pixelfmt_t  srcfmt,
                         pixelfmt_t  destfmt,
                         pixelmap_t *out);

/**
 * Allocate and fill `pm->entries` from `palette`, using the channel layout
 * already set in `pm` by \ref pixelmap__layout_for.
 *
 * \param[in,out] pm       Pixelmap with its layout filled in.
 * \param[in]     palette  Destination palette.
 * \param[in]     nentries Number of entries in `palette`.
 * \return 0 on success, -1 on allocation failure.
 */
int pixelmap__build_table(pixelmap_t     *pm,
                          const colour_t *palette,
                          int             nentries);

/* ----------------------------------------------------------------------- */

#endif /* PIXELMAP_IMPL_H */
