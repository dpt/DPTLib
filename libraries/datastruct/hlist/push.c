/* datastruct/hlist/push.c */

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_push(const alloc_t *alloc, T list, void *x)
{
  T p;

  if (alloc == NULL)
    alloc = &alloc_default;

  p = alloc->malloc(sizeof(*p));
  if (p == NULL)
    return NULL; /* the caller still holds 'list' */

  p->first = x;
  p->rest = list;

  return p;
}
