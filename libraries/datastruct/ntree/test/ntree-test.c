
#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/suppress.h"
#include "utils/array.h"

#include "datastruct/ntree.h"

/* Test Tree 1
 *
 * 1  (a)------+
 *     |       |
 * 2  (b)--+  (c)--+---+
 *     |   |   |   |   |
 * 3  (d) (e) (f) (g) (h)
 *     |           |
 * 4  (i)         (j)
 */

typedef struct mynode
{
  char       *data;
  int         parent;  /* index */
  int         position;
  ntree_t    *node;
}
mynode_t;

static mynode_t data[] =
{
  { "a", -1, 0, NULL },
  { "b",  0, 0, NULL },
  { "c",  0, 1, NULL },
  { "d",  1, 0, NULL },
  { "e",  1, 1, NULL },
  { "f",  2, 0, NULL },
  { "g",  2, 1, NULL },
  { "h",  2, 2, NULL },
  { "i",  3, 0, NULL },
  { "j",  6, 0, NULL },
};

static const char *expected_in_order[6] =
{
  "idbeafcjgh",
  "a",
  "bac",
  "dbeafcgh",
  "idbeafcjgh",
  "idbeafcjgh",
};

static const char *expected_pre_order[6] =
{
  "abdiecfgjh",
  "a",
  "abc",
  "abdecfgh",
  "abdiecfgjh",
  "abdiecfgjh",
};

static const char *expected_post_order[6] =
{
  "idebfjghca",
  "a",
  "bca",
  "debfghca",
  "idebfjghca",
  "idebfjghca",
};

typedef struct print_data
{
  char *buf;
  int   index;
}
print_data_t;

static result_t concat(ntree_t *t, void *opaque)
{
  print_data_t *data = opaque;
  char         *s;

  s = ntree_get_data(t);

  strcat(data->buf, s);
  data->index += strlen(s);

  printf("'%s' at depth %d\n", s, ntree_depth(t));

  return result_OK;
}

static result_t tree_to_string(ntree_t *t, ntree_walk_flags flags, int depth,
                               char *buf)
{
  result_t     err;
  print_data_t data;

  buf[0] = '\0';

  data.buf   = buf;
  data.index = 0;

  err = ntree_walk(t, flags | ntree_WALK_ALL, depth, concat, &data);

  return err;
}

static result_t walk_test(ntree_t *t, ntree_walk_flags flags,
                          const char *expected[])
{
  result_t err;
  int   i;

  for (i = 0; i < 6; i++) /* test outside the range 1..4 */
  {
    char output[100];

    output[0] = '\0';

    err = tree_to_string(t, flags, i, output);
    if (err)
      goto Failure;

    printf("walking depth %d returned: %s\n", i, output);

    if (strcmp(expected[i], output) != 0)
    {
      printf("failure: unexpected output!\n");
      err = -1;
      goto Failure;
    }
  }

  return result_OK;


Failure:

  return err;
}

static result_t dup_data(void *data, void *opaque, void **newdata)
{
  size_t len;
  char  *cpy;

  NOT_USED(opaque);

  len = strlen(data);
  cpy = malloc(len + 1);
  if (cpy)
    memcpy(cpy, data, len + 1);

  *newdata = cpy;

  return result_OK;
}

static result_t free_data(ntree_t *t, void *opaque)
{
  NOT_USED(opaque);

  free(ntree_get_data(t));

  return result_OK;
}

result_t ntree_test(void)
{
  result_t err;
  int      i;

  printf("test: build tree\n");

  for (i = 0; i < NELEMS(data); i++)
  {
    err = ntree_new(&data[i].node);
    if (err)
      goto Failure;

    ntree_set_data(data[i].node, data[i].data);

    if (data[i].parent >= 0) /* skip root */
    {
      ntree_t *parent;
      int      position;

      parent   = data[data[i].parent].node;
      position = data[i].position;

      err = ntree_insert(parent, position, data[i].node);
      if (err)
        goto Failure;
    }
  }

  printf("test: walk in order\n");

  err = walk_test(data[0].node, ntree_WALK_IN_ORDER, expected_in_order);
  if (err)
    goto Failure;

  printf("test: walk pre order\n");

  err = walk_test(data[0].node, ntree_WALK_PRE_ORDER, expected_pre_order);
  if (err)
    goto Failure;

  printf("test: walk post order\n");

  err = walk_test(data[0].node, ntree_WALK_POST_ORDER, expected_post_order);
  if (err)
    goto Failure;

  printf("test: max height\n");

  printf("height = %d\n", ntree_max_height(data[0].node));

  printf("test: number of nodes\n");

  printf("nodes = %d\n", ntree_n_nodes(data[0].node));

  printf("test: copy tree\n");

  {
    ntree_t *copy;

    printf("sub-test: copy\n");

    err = ntree_copy(data[0].node, dup_data, NULL, &copy);
    if (err)
      goto Failure;

    printf("sub-test: walk in order\n");

    err = walk_test(copy, ntree_WALK_IN_ORDER, expected_in_order);
    if (err)
      goto Failure;

    printf("sub-test: discard copied data\n");

    err = ntree_walk(copy, ntree_WALK_IN_ORDER | ntree_WALK_ALL, 0,
                     free_data, NULL);
    if (err)
      goto Failure;

    printf("sub-test: delete tree\n");

    ntree_delete(copy);
  }

  printf("test: delete tree\n");

  ntree_delete(data[0].node);

  return result_TEST_PASSED;


Failure:

  printf("Error %lu\n", err);

  return result_TEST_FAILED;
}
