/* box.h -- box type */

#ifndef GEOM_BOX_H
#define GEOM_BOX_H

#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __riscos

/** A box inclusive of (x0,y0) and exclusive of (x1,y1). */
typedef struct box
{
  int x0, y0, x1, y1;
}
box_t;

#else

#include "oslib/os.h"
/* When on RISC OS, use OSLib's box type in preference. */
typedef os_box box_t;

#endif

#define BOX_RESET { INT_MAX, INT_MAX, INT_MIN, INT_MIN }

/**
 * Reset the box to an invalid state.
 *
 * This sets x0,y0 to INT_MAX and the x1,y1 to INT_MIN. This is an invalid
 * box but will still produce a valid result when intersected with.
 *
 * \param[in] b The box to reset.
 */
void box_reset(box_t *b);

/**
 * Affirms if box "outer" entirely contains box "inner".
 *
 * \param[in] inner The inner box.
 * \param[in] outer The outer box.
 *
 * \return Non-zero if the box is contained.
 */
int box_contains_box(const box_t *inner, const box_t *outer);

/**
 * Affirms if the box "b" entirely contains the point (x,y).
 *
 * \param[in] b The box to test.
 * \param[in] x The x coordinate of the point to test.
 * \param[in] y The y coordinate of the point to test.
 *
 * \return Non-zero if the point is contained.
 */
int box_contains_point(const box_t *b, int x, int y);

/**
 * Affirms if the box "a" intersects with box "b".
 *
 * \param[in]  a The first box.
 * \param[in]  b The second box.
 *
 * \return Non-zero if the boxes intersect.
 */
int box_intersects(const box_t *a, const box_t *b);

/**
 * Populates the box "c" with the intersection of boxes "a" and "b".
 *
 * \param[in]  a The first box.
 * \param[in]  b The second box.
 * \param[out] c The output intersected box.
 *
 * \return Non-zero if the result is invalid.
 */
int box_intersection(const box_t *a, const box_t *b, box_t *c);

/**
 * Populates the box "clipped" with the sizes of the edges discarded when clipping box "b" against "a".
 *
 * \param[in]  a The first box.
 * \param[in]  b The second box.
 * \param[out] clipped Not really a box, but one scalar per edge. Values are positive where "b" extends outside of "a", zero otherwise.
 */
void box_clipped(const box_t *a, const box_t *b, box_t *clipped);

/**
 * Populates the box "c" with the union of boxes "a" and "b".
 *
 * \param[in]  a The first box.
 * \param[in]  b The second box.
 * \param[out] c The output unioned box.
 */
void box_union(const box_t *a, const box_t *b, box_t *c);

/**
 * Affirms if the specified box is empty.
 *
 * \param[in] b The box to test.
 *
 * \return Non-zero if the box is empty.
 */
int box_is_empty(const box_t *b);

/**
 * Grows the box outwards by "change".
 *
 * \param[in] b The box to grow.
 */
void box_grow(box_t *b, int change);

/**
 * Rounds a box's coordinates so that they're a multiple of log2 x,y.
 *
 * x0 and y0 are rounded down. x1 and y1 are rounded up.
 *
 * \param[in] b The box to round.
 */
void box_round(box_t *b, int log2x, int log2y);

/**
 * Rounds a box's coordinates so that they're a multiple of 4.
 *
 * x0 and y0 are rounded down. x1 and y1 are rounded up.
 *
 * \param[in] b The box to round.
 */
void box_round4(box_t *b);

/**
 * Affirms if the box "b" is at least (w,h) in size.
 *
 * \param[in] b The box to test.
 * \param[in] w The width to test.
 * \param[in] h The height to test.
 *
 * \return Non-zero if the point is contained.
 */
int box_could_hold(const box_t *b, int w, int h);

/**
 * Translates box "b" by (x,y) producing new box "t".
 *
 * \param[in] b The box to translate.
 * \param[in] x The amount to translate by horizontally.
 * \param[in] y The amount to translate by vertically.
 * \param[in] t The new box.
 */
void box_translated(const box_t *b, int x, int y, box_t *t);

/**
 * Extend box "b" to include the point (x,y).
 *
 * \param[in] b The box to extend.
 * \param[in] x The x coordinate of the point to include.
 * \param[in] y The y coordinate of the point to include.
 */
void box_extend(box_t *b, int x, int y); // not sure about name

/**
 * Extend box "b" to include any number of points.
 *
 * \param[in] b The new box.
 * \param[in] npoints Numer of points supplied.
 * \param[in] varargs Two ints for every point.
 */
void box_extend_n(box_t *b, int npoints, ...);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_BOX_H */
