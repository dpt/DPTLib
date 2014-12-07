
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_list(void *x, ...)
{
  va_list ap;
  T       list;
  T      *p;

  p = &list;

  va_start(ap, x);

  for ( ; x; x = va_arg(ap, void *))
  {
    *p = malloc(sizeof(**p));
    if (*p == NULL)
      return NULL; // doesn't unwind in failure case

    (*p)->first = x;
    p = &(*p)->rest;
  }

  *p = NULL;

  va_end(ap);

  return list;
}
