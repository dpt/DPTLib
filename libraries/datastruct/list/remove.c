
#include <assert.h>

#include "datastruct/list.h"

void list_remove(list_t *anchor, list_t *doomed)
{
  list_t *e;

  for (e = anchor; e->next != doomed; e = e->next)
    assert(e != NULL); /* if we hit NULL then throw a wobbly */

  /* 'e' is the element preceding the one to remove */

  e->next = doomed->next; /* == e->next->next; */
}
