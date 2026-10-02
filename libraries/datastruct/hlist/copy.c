/* datastruct/hlist/copy.c */

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

T hlist_copy(T list)
{
  T  head;
  T *p;

  p = &head;

  for (; list; list = list->rest)
  {
    *p = malloc(sizeof(**p));
    if (*p == NULL)
    {
      hlist_free(&head); /* *p is NULL, so this ends the partial list */
      return NULL;
    }

    (*p)->first = list->first;
    p = &(*p)->rest;
  }

  *p = NULL;

  return head;
}
