/* geom/hull.h -- convex hulls */

#ifndef GEOM_HULL_H
#define GEOM_HULL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "geom/point.h"

/**
 * Compute the convex hull of a set of points, by Andrew's monotone chain.
 *
 * The hull is written as a closed polyline, its first point repeated at the
 * end, ready for drawing as a line strip. Collinear points along an edge are
 * dropped. Fewer than three points, or all points collinear, give the span
 * between the extreme points, still closed.
 *
 * \param[in,out] pts  Points to wrap. Sorted in place by x, then y.
 * \param[in]     n    Number of points.
 * \param[out]    hull Hull points. Room for 2 * n + 1 is always enough.
 * \return Number of points written to `hull`; 0 if `n` is 0.
 */
int hull_convex(point_t *pts, int n, point_t *hull);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_HULL_H */
