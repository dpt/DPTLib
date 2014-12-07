
#include <stdlib.h>
#include <string.h>

#include "datastruct/array.h"

void array_delete_element(void  *array,
                          size_t elemsize,
                          int    nelems,
                          int    doomed)
{
  /* alternative:
     array_delete_elements(array, elemsize, nelems, doomed, doomed); */

  size_t n;
  char  *to;
  char  *from;

  n = nelems - (doomed + 1);
  if (n == 0)
    return;

  to   = (char *) array + elemsize * doomed;
  from = (char *) array + elemsize * (doomed + 1);

  memmove(to, from, n * elemsize);
}
