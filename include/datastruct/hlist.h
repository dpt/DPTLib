/* datastruct/hlist.h -- "Hanson" linked list library */

/* See chapter 5 of C Interfaces and Implementations. */

#ifndef DATASTRUCT_HLIST_H
#define DATASTRUCT_HLIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "utils/alloc.h"

#define T hlist_t

typedef struct hlist *T;

struct hlist
{
  T     rest;
  void *first; /* the payload */
};

/* Nodes hold no allocator. Each allocating call takes one (NULL for the
 * default) and a list must be freed with the allocator that made it. The
 * array from hlist_to_array is freed with the same allocator. */

T hlist_append(T list, T tail);
T hlist_copy(const alloc_t *alloc, T list);
T hlist_list(const alloc_t *alloc, void *x, ...);
T hlist_pop(const alloc_t *alloc, T list, void **x);
T hlist_push(const alloc_t *alloc, T list, void *x);
T hlist_reverse(T list);
int hlist_length(T list);
void hlist_free(const alloc_t *alloc, T *list);
void hlist_map(T list, void apply(void **x, void *cl),  void *cl);
void **hlist_to_array(const alloc_t *alloc, T list, void *end);

#undef T

#ifdef __cplusplus
}
#endif

#endif /* DATASTRUCT_HLIST_H */
