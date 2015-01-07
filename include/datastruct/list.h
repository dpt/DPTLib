/* list.h -- linked lists */

/**
 * \file list.h
 *
 * A linked list.
 */

#ifndef DATASTRUCT_LIST_H
#define DATASTRUCT_LIST_H

#include <stdlib.h>

#define T list_t

typedef struct list
{
  struct list *next;
}
T;

void list_init(T *anchor);

/* Anchor is assumed to be a static element whose only job is to point to
 * the first element in the list. */

void list_add_to_head(T *anchor, T *item);

void list_remove(T *anchor, T *doomed);

typedef int (list_walk_callback_t)(T *, void *);

int list_walk(T *anchor, list_walk_callback_t *cb, void *opaque);

/* Searches the linked list looking for a key. The key is specified as an
 * offset from the start of the linked list element. It is an int-sized unit.
 */
T *list_find(T *anchor, size_t keyloc, int key);

#undef T

#endif /* DATASTRUCT_LIST_H */
