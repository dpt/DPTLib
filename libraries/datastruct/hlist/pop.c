
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_pop(T list, void **x)
{
  if (list)
  {
    T head = list->rest;
    if (x)
      *x = list->first;
    free(list);
    return head;
  }
  else
  {
    return list;
  }
}
