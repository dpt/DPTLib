/* packerh -- box packing for layout */

#ifndef GEOM_PACKER_H
#define GEOM_PACKER_H

#include "base/result.h"
#include "geom/box.h"

/* ----------------------------------------------------------------------- */

#define result_PACKER_DIDNT_FIT (result_BASE_PACKER + 0)
#define result_PACKER_EMPTY     (result_BASE_PACKER + 1)

/* ----------------------------------------------------------------------- */

#define T packer_t

typedef struct packer T;

T *packer_create(const box_t *dims);
void packer_destroy(T *doomed);

void packer_set_margins(T *packer, const box_t *margins);

typedef enum packer_loc
{
  packer_LOC_TOP_LEFT,
  packer_LOC_TOP_RIGHT,
  packer_LOC_BOTTOM_LEFT,
  packer_LOC_BOTTOM_RIGHT,
  packer_LOC__LIMIT,
}
packer_loc;

/* returns the width of the next available area. */
int packer_next_width(T *packer, packer_loc loc);

/* places an absolutely positioned box 'area'. ignores any margins. */
result_t packer_place_at(T           *packer,
                         const box_t *area);

/* places a box of dimensions (w,h) in the next free area determined by
 * location 'loc'. */
result_t packer_place_by(T            *packer,
                         packer_loc    loc,
                         int           w,
                         int           h,
                         const box_t **pos);

typedef enum packer_cleardir
{
  packer_CLEAR_LEFT,
  packer_CLEAR_RIGHT,
  packer_CLEAR_BOTH,
  packer_CLEAR__LIMIT,
}
packer_cleardir;

/* clears up to the next specified boundary. */
result_t packer_clear(T *packer, packer_cleardir clear);

typedef result_t (packer_map_fn)(const box_t *area, void *opaque);
/* calls 'fn' for every area known about. */
result_t packer_map(T *packer, packer_map_fn *fn, void *opaque);

/* returns the union of all areas used. ignores margins. */
const box_t *packer_get_consumed_area(const T *packer);

#undef T

#endif /* GEOM_PACKER_H */
