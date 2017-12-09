
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

void hlist_map(T list, void apply(void **x, void *cl), void *cl)
{
  T next;

  assert(apply != NULL);

  /* Difference from CII: a next pointer is maintained for some robustness.
   */

  for (; list; list = next)
  {
    next = list->rest;
    apply(&list->first, cl);
  }
}
