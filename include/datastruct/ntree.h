/* ntree.h -- n-ary trees */

/**
 * \file ntree.h
 *
 * NTree is an N-ary tree.
 */

#ifndef DATASTRUCT_NTREE_H
#define DATASTRUCT_NTREE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#define T ntree_t

typedef struct ntree T;

/* ----------------------------------------------------------------------- */

result_t ntree_new(T **t);

/* Unlinks the specified node from the tree. */
void ntree_unlink(T *t);

/* Deletes the specified node and all children. */
void ntree_free(T *t);

/* Unlinks the specified node from the tree, deletes it and all children. */
void ntree_delete(T *t);

/* ----------------------------------------------------------------------- */

T *ntree_nth_child(T *t, int n);

/* ----------------------------------------------------------------------- */

#define ntree_INSERT_AT_END -1

result_t ntree_prepend(T *parent, T *node);
result_t ntree_append(T *parent, T *node);
result_t ntree_insert_before(T *parent, T *sibling, T *node);
result_t ntree_insert_after(T *parent, T *sibling, T *node);

result_t ntree_insert(T *t, int where, T *node);

void ntree_set_data(T *t, void *data);
void *ntree_get_data(T *t);

int ntree_depth(T *t);
int ntree_max_height(T *t);
int ntree_n_nodes(T *t);

T *ntree_next_sibling(T *t);
T *ntree_prev_sibling(T *t);
T *ntree_parent(T *t);
T *ntree_first_child(T *t);
T *ntree_last_child(T *t);

/* ----------------------------------------------------------------------- */

typedef unsigned int ntree_walk_flags_t;

#define ntree_WALK_ORDER_MASK (3u << 0)
#define ntree_WALK_IN_ORDER   (0u << 0)
#define ntree_WALK_PRE_ORDER  (1u << 0)
#define ntree_WALK_POST_ORDER (2u << 0)

#define ntree_WALK_LEAVES     (1u << 2)
#define ntree_WALK_BRANCHES   (1u << 3)
#define ntree_WALK_ALL        (ntree_WALK_LEAVES | ntree_WALK_BRANCHES)

typedef result_t (ntree_walk_fn_t)(T *t, void *opaque);

/* max_depth of 0 means 'walk all', 1..N just walk level 1..N */
result_t ntree_walk(T                  *t,
                    ntree_walk_flags_t  flags,
                    int                 max_depth,
                    ntree_walk_fn_t    *fn,
                    void               *opaque);

/* ----------------------------------------------------------------------- */

typedef result_t (ntree_copy_fn_t)(void *data, void *opaque, void **newdata);

result_t ntree_copy(T *t, ntree_copy_fn_t *fn, void *opaque, T **new_t);

#undef T

#ifdef __cplusplus
}
#endif

#endif /* DATASTRUCT_NTREE_H */
