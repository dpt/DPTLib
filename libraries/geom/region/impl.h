/* geom/region/impl.h -- private region definitions */

#ifndef GEOM_REGION_IMPL_H
#define GEOM_REGION_IMPL_H

#include "geom/region.h"

struct region
{
  box_t bounds;
  int  *start; /* start[r]..start[r+1] index runs[] for scanline y0 + r */
  int  *runs;  /* pairs x0,x1 */
};

/* Builds a region one scanline at a time, top to bottom. */
typedef struct
{
  int *start;
  int *runs;
  int  n, cap; /* ints used and allocated in runs */
  int  y0, h;  /* the scanlines being built */
  int  rowstart;
}
region__builder_t;

/* Begin building scanlines y0..y0+h-1. */
result_t region__builder_init(region__builder_t *b, int y0, int h);

/* Begin scanline y; call for every scanline in order. */
void region__builder_row(region__builder_t *b, int y);

/* Add a run to the current scanline, in ascending order. A run touching or
 * overlapping the previous one extends it. */
result_t region__builder_add(region__builder_t *b, int x0, int x1);

/* Finish: trims empty rows and takes ownership of the buffers. *out is NULL
 * if nothing was added. Also frees the builder, so do not abort after. */
result_t region__builder_finish(region__builder_t *b, region_t **out);

/* Discard the builder's buffers. */
void region__builder_abort(region__builder_t *b);

#endif /* GEOM_REGION_IMPL_H */
