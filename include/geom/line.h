/* line.h -- lines */

#ifndef GEOM_LINE_H
#define GEOM_LINE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "geom/box.h"

/**
 * Clips the line (x0,y0)-(x1,y1) by box `clip` and returns the clipped points
 * in `x0` and co.
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
