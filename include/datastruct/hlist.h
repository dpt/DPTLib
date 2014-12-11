/* hlist.h -- "Hanson" linked list library */

/* See chapter 5 of C Interfaces and Implementations. */

#ifndef DATASTRUCT_HLIST_H
#define DATASTRUCT_HLIST_H

#define T hlist_t

typedef struct hlist *T;

struct hlist
{
  T     rest;
  void *first; /* the payload */
};

T hlist_append(T list, T tail);
T hlist_copy(T list);
T hlist_list(void *x, ...);
T hlist_pop(T list, void **x);
T hlist_push(T list, void *x);
T hlist_reverse(T list);
int hlist_length(T list);
void hlist_free(T *list);
void hlist_map(T list, void apply(void **x, void *cl),  void *cl);
void **hlist_to_array(T list, void *end);

#undef T

#endif /* DATASTRUCT_HLIST_H */
