/* geom/orient.h -- flips and quarter turns of a rectangular area */

#ifndef GEOM_ORIENT_H
#define GEOM_ORIENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/utils.h"

#include "geom/size.h"

/** The ways an area can be flipped or turned. */
typedef enum orient
{
  orient_FLIP_H,     /**< Mirror left to right. */
  orient_FLIP_V,     /**< Mirror top to bottom. */
  orient_ROTATE_CW,  /**< Turn 90 degrees clockwise. */
  orient_ROTATE_CCW, /**< Turn 90 degrees anticlockwise. */
  orient_ROTATE_180, /**< Turn 180 degrees. */
  orient__LIMIT
}
orient_t;

/**
 * The size of an area of `size` once flipped or turned by `orient`.
 *
 * \param[in] orient Flip or turn.
 * \param[in] size   Size of the area before.
 * \return Size of the area after.
 */
static INLINE size2d_t orient_size(orient_t orient, size2d_t size)
{
  if (orient == orient_ROTATE_CW || orient == orient_ROTATE_CCW)
    return SIZE2D(size.h, size.w);
  return size;
}

/**
 * The pixel of a `w` by `h` area which its pixel (`x`, `y`) takes once the
 * area is flipped or turned by `orient`.
 *
 * \param[in]  orient Flip or turn.
 * \param[in]  w      Width of the area before.
 * \param[in]  h      Height of the area before.
 * \param[in]  x      X of a pixel in the area after.
 * \param[in]  y      Y of a pixel in the area after.
 * \param[out] sx     X of the pixel in the area before.
 * \param[out] sy     Y of the pixel in the area before.
 */
static INLINE void orient_source(orient_t orient,
                                 int      w,
                                 int      h,
                                 int      x,
                                 int      y,
                                 int     *sx,
                                 int     *sy)
{
  switch (orient)
  {
  case orient_FLIP_H:     *sx = w - 1 - x; *sy = y;         break;
  case orient_FLIP_V:     *sx = x;         *sy = h - 1 - y; break;
  case orient_ROTATE_CW:  *sx = y;         *sy = h - 1 - x; break;
  case orient_ROTATE_CCW: *sx = w - 1 - y; *sy = x;         break;
  default:                *sx = w - 1 - x; *sy = h - 1 - y; break;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* GEOM_ORIENT_H */
