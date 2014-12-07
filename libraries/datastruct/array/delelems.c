
#include <stdlib.h>
#include <string.h>

#include "datastruct/array.h"

void array_delete_elements(void  *array,
                           size_t elemsize,
                           int    nelems,
                           int    first_doomed,
                           int    last_doomed)
{
  size_t n;
  char  *to;
  char  *from;

  n = nelems - (last_doomed + 1);
  if (n == 0)
    return;

  to   = (char *) array + elemsize * first_doomed;
  from = (char *) array + elemsize * (last_doomed + 1);

  memmove(to, from, n * elemsize);
}
