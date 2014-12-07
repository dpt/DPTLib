
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_append(T list, T tail)
{
  T *p = &list;

  while (*p)
    p = &(*p)->rest;

  *p = tail;

  return list;
}
