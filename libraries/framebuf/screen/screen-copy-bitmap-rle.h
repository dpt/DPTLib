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
 * v1 constraints, else \ref result_NOT_SUPPORTED: - screen must be 32bpp
 * (`pixelfmt_log2bpp(scr->format) == 5`); - `pixelfmt_base(src->format)`
 * must equal `scr->format` (byte copy, no per-pixel conversion); - blit is
 * alpha-tested (transparent runs skipped, opaque runs copied), not
 * alpha-blended.
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
