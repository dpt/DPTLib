/* impl.h -- box packing for layout */

#ifndef IMPL_H
#define IMPL_H

#include "geom/packer.h"

typedef enum packer_sortdir
{
  packer_SORT_TOP_LEFT,
  packer_SORT_TOP_RIGHT,
  packer_SORT_BOTTOM_LEFT,
  packer_SORT_BOTTOM_RIGHT,
  packer_SORT__LIMIT,
}
packer_sortdir;

struct packer_t
{
  box_t         *areas;
  int            allocedareas;
  int            usedareas;

  box_t          dims;          /* page size */

  box_t          margins;       /* page size minus margins (but not the
                                   margins themselves) */

  int            nextindex;
  box_t          nextarea;      /* area returned by packer_next */

  box_t          placed_area;   /* last packer_place_by result */

  packer_sortdir order;         /* order to which we have sorted */
  int            sorted;        // bool

  box_t          consumed_area; /* total consumed area */
};

#endif /* IMPL_H */
