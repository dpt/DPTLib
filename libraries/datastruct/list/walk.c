
#include "datastruct/list.h"

int list_walk(list_t *anchor, list_walk_callback_t cb, void *opaque)
{
  list_t *e;
  list_t *next;
  int     rc;

  /* Be careful walking the list. The callback may be destroying the objects
   * we're handling to it.
   */

  for (e = anchor->next; e != NULL; e = next)
  {
    next = e->next;
    if ((rc = cb(e, opaque)) < 0)
      return rc;
  }

  return 0;
}
