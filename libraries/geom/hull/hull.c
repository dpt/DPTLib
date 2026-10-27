/* geom/hull/hull.c -- convex hulls */

#include <stddef.h>
#include <stdlib.h>

#include "geom/point.h"

#include "geom/hull.h"

/* ----------------------------------------------------------------------- */

/* 2D cross product of (b - a) and (c - a); > 0 for a left (counter-clockwise)
 * turn, < 0 for a right turn, 0 if collinear. long to keep the products of
 * two point coordinates from overflowing an int. */
static long hull_cross(point_t a, point_t b, point_t c)
{
  return (long) (b.x - a.x) * (c.y - a.y) - (long) (b.y - a.y) * (c.x - a.x);
}

static int hull_point_cmp(const void *va, const void *vb)
{
  const point_t *a = va;
  const point_t *b = vb;

  if (a->x != b->x)
    return (a->x > b->x) - (a->x < b->x);
  return (a->y > b->y) - (a->y < b->y);
}

int hull_convex(point_t *pts, int n, point_t *hull)
{
  int k;
  int i;
  int lower;

  if (n <= 0)
    return 0;

  qsort(pts, (size_t) n, sizeof(*pts), hull_point_cmp);

  k = 0;
  for (i = 0; i < n; i++) /* lower hull */
  {
    while (k >= 2 && hull_cross(hull[k - 2], hull[k - 1], pts[i]) <= 0)
      k--;
    hull[k++] = pts[i];
  }

  lower = k + 1;
  for (i = n - 2; i >= 0; i--) /* upper hull */
  {
    while (k >= lower && hull_cross(hull[k - 2], hull[k - 1], pts[i]) <= 0)
      k--;
    hull[k++] = pts[i];
  }

  /* a single point never entered the upper loop: close it by hand */
  if (n == 1)
    hull[k++] = pts[0];

  return k; /* hull[0] == hull[k - 1], a closed loop */
}
