/* datastruct/hlist/free.c */

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

void hlist_free(const alloc_t *alloc, T *list)
{
  T next;

  assert(list != NULL);

  if (alloc == NULL)
    alloc = &alloc_default;

  for (; *list; *list = next)
  {
    next = (*list)->rest;
    alloc->free(*list);
  }
}
