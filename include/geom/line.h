/* geom/line.h -- lines */

#ifndef GEOM_LINE_H
#define GEOM_LINE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "geom/box.h"

/**
 * Clips the line (x0,y0)-(x1,y1) by box `clip` and returns the clipped
 * points in `x0` and co.
 *
 * The returned points are rounded to the nearest integer position on the
 * clip boundary, so they vary with the clip box given. Callers which step
 * along the line incrementally (Bresenham, Wu, etc.) must not seed their
 * error terms from them if the pixels drawn are to be independent of the
 * clip box passed in; use the return value to reject and step from the
 * original endpoints.
 *
 * \param[in]     clip Rectangular clip region.
 * \param[in,out] x0   X coordinate of first point of line (modified).
 * \param[in,out] y0   Y coordinate of first point of line (modified).
 * \param[in,out] x1   X coordinate of second point of line (modified).
 * \param[in,out] y1   Y coordinate of second point of line (modified).
 * \return Non-zero if the line was clipped.
 */
int line_clip(const box_t *clip,
              int         *x0,
              int         *y0,
              int         *x1,
              int         *y1);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_LINE_H */
