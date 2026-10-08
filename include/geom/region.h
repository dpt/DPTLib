/* geom/region.h -- sparse sets of integer pixels held as per-row runs */

#ifndef GEOM_REGION_H
#define GEOM_REGION_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "geom/box.h"
#include "geom/point.h"

/**
 * \file region.h
 *
 * A region is a set of pixels, stored as sorted, disjoint, non-touching
 * horizontal runs [x0,x1) on each scanline. Storage and most operations cost
 * in proportion to the number of runs, not the area, so a large rectangle or
 * a smooth lasso is cheap.
 *
 * An empty region is represented by NULL, and every function here accepts
 * NULL as an empty region.
 */

/** An opaque region. */
typedef struct region region_t;

/**
 * Creates a region from a box (x1,y1 exclusive).
 *
 * \param[in]  box The box to fill.
 * \param[out] out The new region, or NULL if the box is empty.
 * \return result_OK or result_OOM.
 */
result_t region_from_box(const box_t *box, region_t **out);

/**
 * Creates a region from the area enclosed by a closed polygon, using the
 * even-odd rule. Pixels on the left and right crossings are included.
 *
 * The cost is O(edges log edges + runs), not O(edges * height).
 *
 * \param[in]  pts  The polygon's vertices (the last joins the first).
 * \param[in]  n    The number of vertices.
 * \param[in]  clip Pixels outside this box (x1,y1 exclusive) are dropped.
 * \param[out] out  The new region, or NULL if no pixels are enclosed.
 * \return result_OK or result_OOM.
 */
result_t region_from_polygon(const point_t *pts,
                             int            n,
                             const box_t   *clip,
                             region_t     **out);

/** Frees a region. */
void region_destroy(region_t *region);

/**
 * Returns the box (x1,y1 exclusive) enclosing the region, or an empty box if
 * the region is empty.
 */
box_t region_bounds(const region_t *region);

/** Returns non-zero if the pixel (x,y) is in the region. */
int region_contains(const region_t *region, int x, int y);

/**
 * Returns the runs on scanline y, as pairs x0,x1 (x1 exclusive) in ascending
 * order.
 *
 * \param[in]  region The region.
 * \param[in]  y      The scanline.
 * \param[out] runs   The pairs of values. Valid until the region is
 *                    destroyed.
 * \return The number of runs (0 if none).
 */
int region_row(const region_t *region, int y, const int **runs);

/**
 * Returns the pixels of the region that have a 4-neighbour outside it, as
 * single-row boxes (x1,y1 exclusive).
 *
 * The cost is O(boundary). A pixel may be reported more than once.
 *
 * \param[in]  region The region.
 * \param[out] spans  A malloc'd array of boxes (NULL if none).
 * \param[out] n      The number of boxes.
 * \return result_OK or result_OOM.
 */
result_t region_outline(const region_t *region, box_t **spans, int *n);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_REGION_H */
