/* geom/size.h -- size type */

#ifndef GEOM_SIZE_H
#define GEOM_SIZE_H

/** 2D size with integer dimensions. */
typedef struct size2d
{
  int w, h;
}
size2d_t;

/** Construct a size2d_t compound literal. */
#define SIZE2D(w, h) ((size2d_t) { (w), (h) })

#endif /* GEOM_SIZE_H */
