/* datastruct/hlist/pop.c */

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_pop(const alloc_t *alloc, T list, void **x)
{
  if (alloc == NULL)
    alloc = &alloc_default;

  if (list)
  {
    T head = list->rest;
    if (x)
      *x = list->first;
    alloc->free(list);
    return head;
  }
  else
  {
    return list;
  }
}
