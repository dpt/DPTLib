/* framebuf/screen/screen-copy-bitmap-rle.h -- RLE bitmap blit (private) */

#ifndef FRAMEBUF_SCREEN_COPY_BITMAP_RLE_H
#define FRAMEBUF_SCREEN_COPY_BITMAP_RLE_H

#include "base/result.h"

#include "geom/box.h"

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

/**
 * Blit an RLE-compressed bitmap onto a screen, top-left at (x, y), clipped
 * to `draw_box` (already intersected with the screen clip).
 *
 * Constraints, else \ref result_NOT_SUPPORTED: - screen must be 32bpp, 16bpp
 * or paletted (1, 2, 4 or 8bpp); - source must decode to 32bpp; - on a 32bpp
 * screen `pixelfmt_base(src->format)` must match the screen's channel order
 * (byte copy, no per-pixel conversion), on a paletted screen each pixel is
 * converted to the nearest palette entry. The blit is alpha-tested
 * (transparent runs skipped, opaque runs written) on 32bpp and paletted
 * screens, and alpha-blended on a 16bpp screen.
 *
 * \param[in] scr      Destination screen.
 * \param[in] x,y      Where the bitmap's top-left lands, screen pixels.
 * \param[in] src      Compressed source bitmap.
 * \param[in] draw_box Visible rectangle in screen pixels.
 * \return \ref result_OK, or \ref result_NOT_SUPPORTED.
 */
result_t screen_copy_bitmap_rle(screen_t       *scr,
                                int             x,
                                int             y,
                                const bitmap_t *src,
                                const box_t    *draw_box);

#endif /* FRAMEBUF_SCREEN_COPY_BITMAP_RLE_H */
