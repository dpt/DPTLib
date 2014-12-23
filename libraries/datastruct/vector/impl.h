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

#endif /* IMPL_H */
