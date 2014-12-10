/* vector.h -- vector - flexible array */

#ifndef IMPL_H
#define IMPL_H

#include <stddef.h>

struct vector
{
  size_t width;     /* width of an element */
  int    used;      /* entries used */
  int    allocated; /* entries allocated */
  void  *base;      /* vector itself */
};

#endif /* IMPL_H */
