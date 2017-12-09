
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_reverse(T list)
{
  T head;
  T next;

  head = NULL;

  for (; list; list = next)
  {
    next = list->rest;
    list->rest = head;
    head = list;
  }

  return head;
}
