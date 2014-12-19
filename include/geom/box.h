/* box.h -- box type */

#ifndef GEOM_BOX_H
#define GEOM_BOX_H

typedef struct box
{
  int x0, y0, x1, y1;
}
box_t;

/**
 * Affirms if box "outside" entirely contains box "inside".
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
 */
void box_intersection(const box_t *a, const box_t *b, box_t *c);

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
 * Rounds a box's coordinates so that they're a multiple of 'amount'.
 *
 * x0 and y0 are rounded down. x1 and y1 are rounded up.
 *
 * \param[in] b The box to round.
 */
void box_round(box_t *b, int amount);

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

#endif /* GEOM_BOX_H */
