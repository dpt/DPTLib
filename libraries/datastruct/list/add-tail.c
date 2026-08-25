
#include "datastruct/list.h"

void list_add_to_tail(list_t *anchor, list_t *item)
{
  list_t *e;

  for (e = anchor; e->next != NULL; e = e->next)
    ;

  e->next    = item;
  item->next = NULL;
}
