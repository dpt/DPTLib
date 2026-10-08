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

#endif /* GEOM_REGION_IMPL_H */
