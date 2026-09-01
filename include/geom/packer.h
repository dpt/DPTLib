/* geom/packer.h -- box packing for layout */

#ifndef GEOM_PACKER_H
#define GEOM_PACKER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"

/* ----------------------------------------------------------------------- */

#define result_PACKER_DIDNT_FIT (result_BASE_PACKER + 0)
#define result_PACKER_EMPTY     (result_BASE_PACKER + 1)

/* ----------------------------------------------------------------------- */

#define T packer_t

typedef struct packer T;

/**
 * Creates a new packer.
 *
 * \param[in] dims Dimensions of the packer.
 * \return New packer.
 */
T *packer_create(const box_t *dims);

/**
 * Destroys a packer.
 *
 * \param[in] doomed Packer to destroy.
 */
void packer_destroy(T *doomed);

/**
 * Sets the margins of the packer.
 *
 * \param[in] packer  Packer to set margins.
 * \param[in] margins Margins to set.
 */
void packer_set_margins(T *packer, const box_t *margins);

/**
 * Direction to search from for the next available area.
 */
typedef enum packer_loc
{
  packer_LOC_TOP_LEFT,
  packer_LOC_TOP_RIGHT,
  packer_LOC_BOTTOM_LEFT,
  packer_LOC_BOTTOM_RIGHT,
  packer_LOC__LIMIT,
}
packer_loc_t;

/**
 * Returns the width of the next available area.
 *
 * \param[in] packer Packer to get width.
 * \param[in] loc    Direction to search from for the next available area.
 * \return Width of the next available area.
 */
int packer_next_width(T *packer, packer_loc_t loc);

/**
 * Places an absolutely positioned box 'area'.
 *
 * \param[in] packer Packer to place box.
 * \param[in] area   Box to place.
 * \return Result of the placement.
 */
result_t packer_place_at(T           *packer,
                         const box_t *area);

/**
 * Sets a gutter for packer_place_by: the width in pixels of a strip it
 * additionally reserves along each placed box's two edges facing away from
 * the search corner, so boxes placed by location never end up flush against
 * each other. Default 0 (no gutter). Negative values are treated as 0. Does
 * not affect packer_place_at.
 *
 * \param[in] packer Packer to configure.
 * \param[in] gutter Gutter width in pixels.
 */
void packer_set_gutter(T *packer, int gutter);

/**
 * Returns a previously placed area to the free pool: the inverse of
 * packer_place_at / packer_place_by.
 *
 * Free areas that share a full edge are coalesced after each release, so
 * repeated place/release cycles reclaim the whole page rather than
 * fragmenting it. A placement spanning two free areas that only partially
 * overlap (staggered edges) still will not fit until the gap between them is
 * freed too. packer_get_consumed_area is not narrowed by a release.
 *
 * \param[in] packer Packer to release into.
 * \param[in] area   Area to release. Clipped to the packer's margins.
 *                   Copied.
 * \return \ref result_OK, or \ref result_PACKER_EMPTY if 'area' lies
 *         entirely outside the margins.
 */
result_t packer_release(T           *packer,
                        const box_t *area);

/**
 * Places a box of dimensions (w,h) in the next free area determined by
 * location 'loc'.
 *
 * \param[in]  packer Packer to place box.
 * \param[in]  loc    Direction to search from for the next available area.
 * \param[in]  w      Width of the box to place.
 * \param[in]  h      Height of the box to place.
 * \param[out] pos    Position of the box.
 * \return Result of the placement.
 */
result_t packer_place_by(T            *packer,
                         packer_loc_t  loc,
                         int           w,
                         int           h,
                         const box_t **pos);

/**
 * Direction to clear (to move past).
 */
typedef enum packer_cleardir
{
  packer_CLEAR_LEFT,
  packer_CLEAR_RIGHT,
  packer_CLEAR_BOTH,
  packer_CLEAR__LIMIT,
}
packer_cleardir_t;

/**
 * Clears up to the next specified boundary.
 *
 * \param[in] packer Packer to clear.
 * \param[in] clear  Direction to clear.
 * \return Result of the clearing.
 */
result_t packer_clear(T *packer, packer_cleardir_t clear);

/**
 * A function to call for every area known about.
 */
typedef result_t (packer_map_fn_t)(const box_t *area, void *opaque);

/**
 * Calls the given function for every area known about.
 *
 * \param[in] packer Packer to query.
 * \param[in] fn     Function to call.
 * \param[in] opaque Opaque pointer to pass to the function.
 * \return Result of the mapping.
 */
result_t packer_map(T *packer, packer_map_fn_t *fn, void *opaque);

/**
 * Returns the union of all areas used (ignoring margins).
 *
 * \param[in] packer Packer to query.
 * \return Union of all areas used.
 */
const box_t *packer_get_consumed_area(const T *packer);

#undef T

#ifdef __cplusplus
}
#endif

#endif /* GEOM_PACKER_H */
