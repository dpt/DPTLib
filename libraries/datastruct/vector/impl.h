/* vector.h -- vector - flexible array */

#ifndef IMPL_H
#define IMPL_H

#include <stddef.h>

struct vector
{
  size_t width;     /* width of an element */
  size_t used;      /* entries used */
  size_t allocated; /* entries allocated */
  void  *base;      /* vector itself */
};

/* Calculate address of element 'i'. */
#define VECTOR_INDEX(v, i) ((char *) v->base + (i) * v->width)

#endif /* IMPL_H */
