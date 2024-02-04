/* span.h -- interface of plotting methods */

#ifndef SPAN_H
#define SPAN_H

#include "framebuf/pixelfmt.h"

typedef void (span_copy_t)(void *dst, const void *src, int length);

typedef void (span_blendconst_t)(void       *dst,
                                 const void *src1,
                                 const void *src2,
                                 int         length,
                                 int         alpha);

typedef void (span_blendarray_t)(void                *dst,
                                 const void          *src1,
                                 const void          *src2,
                                 int                  length,
                                 const unsigned char *alphas);

typedef struct span
{
  pixelfmt_t         format;
  span_copy_t       *copy;
  span_blendconst_t *blendconst;
  span_blendarray_t *blendarray;
}
span_t;

#endif /* SPAN_H */
