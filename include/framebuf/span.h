/* span.h -- interface of plotting methods */

#ifndef SPAN_H
#define SPAN_H

#include "framebuf/pixelfmt.h"

/**
 * Type of a "copy pixels" function.
 *
 * \param[out] dst    Destination pixels.
 * \param[in]  src    Source pixels.
 * \param[in]  length Length of pixels to copy. ( is this number or bytes? )
 */
typedef void (span_copy_t)(void *dst, const void *src, int length);

/**
 * Type of a "blend constant pixels" function.
 *
 * This will blend the respective source pixels by the specified constant alpha value, writing the results to the destination buffer (like Porter-Duff Source Over Destination).
 *
 * \param[out] dst    Destination pixels.
 * \param[in]  src1   Source pixels 1.
 * \param[in]  src2   Source pixels 2.
 * \param[in]  length Length of pixels to blend.
 * \param[in]  alpha  Constant alpha value (0..255).
 */
typedef void (span_blendconst_t)(void       *dst,
                                 const void *src1,
                                 const void *src2,
                                 int         length,
                                 int         alpha);

/**
 * Type of a "blend array of pixels" function.
 *
 * This will blend the respective source pixels by the specified alpha values, writing the results to the destination buffer (like Porter-Duff Source Over Destination).
 *
 * \param[out] dst    Destination pixels.
 * \param[in]  src1   Source pixels 1.
 * \param[in]  src2   Source pixels 2.
 * \param[in]  length Length of pixels to blend.
 * \param[in]  alphas Array of alpha values (0..255).
 */
typedef void (span_blendarray_t)(void                *dst,
                                 const void          *src1,
                                 const void          *src2,
                                 int                  length,
                                 const unsigned char *alphas);

/**
 * Defines a span.
 *
 * A span is a group of functions that combine runs of pixels. They are keyed by pixel format.
 */
typedef struct span
{
  pixelfmt_t         format;     /**< Pixel format this span is for. */
  span_copy_t       *copy;       /**< Copy pixels function. */
  span_blendconst_t *blendconst; /**< Blend constant pixels function. */
  span_blendarray_t *blendarray; /**< Blend array of pixels function. */
}
span_t;

#endif /* SPAN_H */
