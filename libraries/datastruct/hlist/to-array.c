/* datastruct/hlist/to-array.c */

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

void **hlist_to_array(const alloc_t *alloc, T list, void *end)
{
  int    n;
  void **array;
  int    i;

  if (alloc == NULL)
    alloc = &alloc_default;

  n = hlist_length(list);

  array = alloc->malloc((n + 1) * sizeof(*array));
  if (array == NULL)
    return NULL;

  for (i = 0; i < n; i++)
  {
    array[i] = list->first;
    list = list->rest;
  }

  array[i] = end;
  return array;
}
