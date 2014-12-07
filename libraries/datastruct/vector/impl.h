/* --------------------------------------------------------------------------
 *    Name: vector.h
 * Purpose: Vector - flexible array
 * ----------------------------------------------------------------------- */

#ifndef IMPL_H
#define IMPL_H

#include <stddef.h>

struct vector_t
{
  size_t width;     /* width of an element */
  int    used;      /* space used */
  int    allocated; /* space allocated */
  void  *base;      /* vector itself */
};

#endif /* IMPL_H */
