/* framebuf/effects/impl.h -- private declarations for bitmap effects */

#ifndef EFFECTS_IMPL_H
#define EFFECTS_IMPL_H

#include <stddef.h>
#include <stdint.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

/* ----------------------------------------------------------------------- */

/** Bit position of each byte within a 32bpp pixel word. */
typedef struct effects_layout
{
  unsigned int r, g, b;
  unsigned int a; /* alpha, or padding for the x8888 formats */
}
effects_layout_t;

/** Pointer to the first pixel of row `y` of a 32bpp bitmap. */
#define EFFECTS_ROW(bm, y) \
  ((uint32_t *) ((char *) (bm)->base + (size_t) (y) * (bm)->rowbytes))

/** Colour channel mask for a layout (everything but the alpha byte). */
#define EFFECTS_ALPHA_MASK(l) (0xFFu << (l)->a)

/**
 * Describe the byte order of a 32bpp format.
 *
 * \return \ref result_BAD_ARG if `fmt` is not a 32bpp format.
 */
result_t effects__layout(pixelfmt_t fmt, effects_layout_t *layout);

/**
 * Check that `src` suits an effect and describe its layout.
 *
 * \return \ref result_BAD_ARG if `src` is not a 32bpp bitmap with whole-word
 *         rows.
 */
result_t effects__check_one(const bitmap_t   *src,
                            effects_layout_t *layout);

/**
 * Check that `src` and `dst` suit an effect and describe their layout.
 *
 * \return \ref result_BAD_ARG if either bitmap is not a 32bpp bitmap with
 *         whole-word rows, or the two differ in size or format.
 */
result_t effects__check(const bitmap_t   *src,
                        const bitmap_t   *dst,
                        effects_layout_t *layout);

/** Map each colour channel through a table; copy the fourth byte. */
void effects__remap(const bitmap_t         *src,
                    bitmap_t               *dst,
                    const effects_layout_t *layout,
                    const uint8_t          *red,
                    const uint8_t          *green,
                    const uint8_t          *blue,
                    const uint8_t          *alpha);

/** Count the levels of each colour channel: `hist[0..2]` is R, G, B. */
void effects__histogram(const bitmap_t         *src,
                        const effects_layout_t *layout,
                        unsigned int            hist[3][256]);

/** Copy all pixels of `src` to `dst` (no-op for the same bitmap). */
void effects__copy(const bitmap_t *src, bitmap_t *dst);

/* ----------------------------------------------------------------------- */

typedef struct convolve_lut convolve_lut_t;

/** Build the lookup tables for a symmetric kernel of `n` weights. */
result_t effects__convolve_init(const float     *kernel,
                                int              n,
                                convolve_lut_t **lut);

void effects__convolve_destroy(convolve_lut_t *lut);

/** Convolve every row, then every column, of `src` into `dst`. */
result_t effects__convolve(const convolve_lut_t *lut,
                           const bitmap_t       *src,
                           bitmap_t             *dst);

#endif /* EFFECTS_IMPL_H */
