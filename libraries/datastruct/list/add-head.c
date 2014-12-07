
#include "datastruct/list.h"

void list_add_to_head(list_t *anchor, list_t *item)
{
  item->next = anchor->next;
  anchor->next = item;
}
