/* framebuf/pixelmap.h -- cached bitmap-format conversion tables */

#ifndef FRAMEBUF_PIXELMAP_H
#define FRAMEBUF_PIXELMAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

/* ----------------------------------------------------------------------- */

/**
 * A cached lookup table converting between a "deep" (32bpp) pixel format and
 * a paletted (p1/p2/p4/p8) one, in either direction. It replaces a per-pixel
 * \ref colour_to_pixel call in a blit inner loop with a table read.
 *
 * **Deep source to paletted destination.** \ref colour_to_pixel would run a
 * weighted linear scan of the whole palette per pixel. The pixelmap does
 * that once: the source pixel's red, green and blue channels are extracted
 * with \ref rshift / \ref gshift / \ref bshift, quantised to \ref rbits /
 * \ref gbits / \ref bbits, composed into the index `(r << (gbits + bbits)) |
 * (g << bbits) | b`, and used to read \ref entries. The quantisation trades
 * a little colour accuracy for a table small enough to stay resident (2KB
 * for a 32bpp-to-p4 map). \ref entries is packed to the destination bit
 * width, LSB-first within each byte: p4 holds two 4-bit entries per byte, p2
 * four, p1 eight, p8 one byte per entry. \ref entry_bytes is 0.
 *
 * **Paletted source to deep destination.** The index is the raw palette
 * index (0..\ref nentries - 1); there is no quantisation and
 * rbits/gbits/bbits and the shifts are 0. \ref entries is an array of \ref
 * nentries pixels, each \ref entry_bytes wide (4 for a 32bpp destination),
 * holding the destination pixel for that palette index: `dst = ((const
 * pixelfmt_any32_t *) pm->entries)[palette_index]`.
 *
 * The table is owned by the pixelmap module and lives in a small cache. A
 * caller that blits in a loop calls \ref pixelmap_get once, before the loop.
 */
typedef struct pixelmap
{
  pixelfmt_t           srcfmt;
  pixelfmt_t           destfmt;
  int                  dest_log2bpp;    /* 0..3, deep->paletted only */
  unsigned char        entry_bytes;     /* 0 = packed paletted, else deep */
  unsigned char        rbits, gbits, bbits;
  unsigned char        rshift, gshift, bshift; /* into the source pixel */
  unsigned int         nentries;
  const unsigned char *entries;
}
pixelmap_t;

/* ----------------------------------------------------------------------- */

/**
 * Return a cached table converting a `srcfmt` pixel to `destfmt` under
 * `palette`.
 *
 * Exactly one of `srcfmt` / `destfmt` must be paletted (p1/p2/p4/p8) and the
 * other a 32bpp `*8888` format. `palette` is always the paletted side's
 * palette.
 *
 * The returned pointer is owned by the pixelmap module and stays valid until
 * a later \ref pixelmap_get call with a different key evicts it. Callers
 * that blit in a loop fetch it once, before the loop.
 *
 * \param[in] srcfmt   Source pixel format.
 * \param[in] destfmt  Destination pixel format.
 * \param[in] palette  Palette of the paletted side.
 * \param[in] nentries Number of entries in `palette`.
 * \return A cached table, or NULL if the format pair is unsupported,
 *         `palette` is NULL, or allocation failed.
 */
const pixelmap_t *pixelmap_get(pixelfmt_t      srcfmt,
                               pixelfmt_t      destfmt,
                               const colour_t *palette,
                               int             nentries);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_PIXELMAP_H */
