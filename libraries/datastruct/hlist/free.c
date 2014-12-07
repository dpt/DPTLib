
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

void hlist_free(T *list)
{
  T next;

  assert(list != NULL);

  for ( ; *list; *list = next)
  {
    next = (*list)->rest;
    free(*list);
  }
}
