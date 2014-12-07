
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hlist.h"

#include "impl.h"

int hlist_length(T list)
{
  int n;

  for (n = 0; list; list = list->rest)
    n++;

  return n;
}
