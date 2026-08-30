/* point.h -- point type */

#ifndef GEOM_POINT_H
#define GEOM_POINT_H

/** 2D point with integer coordinates. */
typedef struct point
{
  int x, y;
}
point_t;

/** Construct a point_t compound literal. */
#define POINT(x, y) ((point_t) { (x), (y) })

#endif /* GEOM_POINT_H */
