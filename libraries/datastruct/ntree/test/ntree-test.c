
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "utils/array.h"

#include "datastruct/ntree.h"

/* -------------------------------------------------------------------------- */

#define BEGIN_TEST(testname) \
  do { printf("test: %s\n", testname); } while (0)

#define BEGIN_SUBTEST(subtestname) \
  do { printf("sub-test: %s\n", subtestname); } while (0)

#define TEST_OK \
  do { printf("(ok)\n"); } while (0)

#define TEST_FAILED \
  do { printf("(FAIL)\n"); } while (0)

/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */

typedef struct ntreetest_node
{
  char    *data;     /* string to attach to the node (RO) */
  int      parent;   /* index of parent (RO) */
  int      position; /* where to insert (RO) */
  ntree_t *node;     /* created node (RW) */
}
ntreetest_node_t;

static ntreetest_node_t test_data[] =
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

/* -------------------------------------------------------------------------- */

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

typedef struct concat_data
{
  char *buf;
  int   index;
}
concat_data_t;

static result_t concat(ntree_t *t, void *opaque)
{
  concat_data_t *concat_data = opaque;
  char          *s;

  s = ntree_get_data(t);

  strcat(concat_data->buf, s);
  concat_data->index += strlen(s);

  return result_OK;
}

static result_t tree_to_string(ntree_t            *t,
                               ntree_walk_flags_t  flags,
                               int                 depth,
                               char               *buf)
{
  result_t      err;
  concat_data_t concat_data;

  buf[0] = '\0';

  concat_data.buf   = buf;
  concat_data.index = 0;

  err = ntree_walk(t, flags | ntree_WALK_ALL, depth, concat, &concat_data);

  return err;
}

static result_t walk_test(ntree_t            *t,
                          ntree_walk_flags_t  flags,
                          const char         *expected[])
{
  result_t err = result_OK;
  int      i;

  for (i = 0; i < 6; i++) /* test outside the range 1..4 */
  {
    char output[100];

    output[0] = '\0';

    err = tree_to_string(t, flags, i, output);
    if (err)
      goto fatal;

    printf("walking to depth %d returned: \"%s\"\n", i, output);

    if (strcmp(expected[i], output) != 0)
    {
      err = result_TEST_FAILED; /* note the failure but don't immediately return */
      TEST_FAILED;
    }
    else
    {
      TEST_OK;
    }
  }

fatal:
  return err;
}

/* -------------------------------------------------------------------------- */

static result_t dup_data(void *data, void *opaque, void **newdata)
{
  size_t  len;
  char   *copy;

  NOT_USED(opaque);

  len = strlen(data);
  copy = malloc(len + 1);
  if (copy == NULL)
    return result_OOM;

  memcpy(copy, data, len + 1);

  *newdata = copy;

  return result_OK;
}

/* -------------------------------------------------------------------------- */

static result_t free_data(ntree_t *t, void *opaque)
{
  NOT_USED(opaque);

  free(ntree_get_data(t));

  return result_OK;
}

/* -------------------------------------------------------------------------- */

result_t ntree_test(void); /* suppress "No previous prototype" warning */

result_t ntree_test(void)
{
  result_t err;
  int      nfailures = 0;
  int      i;

  BEGIN_TEST("build tree");

  for (i = 0; i < NELEMS(test_data); i++)
  {
    err = ntree_new(&test_data[i].node);
    if (err)
      goto fatal;

    ntree_set_data(test_data[i].node, test_data[i].data);

    if (test_data[i].parent >= 0) /* skip root */
    {
      ntree_t *parent;
      int      position;

      parent   = test_data[test_data[i].parent].node;
      position = test_data[i].position;

      err = ntree_insert(parent, position, test_data[i].node);
      if (err)
        goto fatal;
    }
  }

  TEST_OK;


  BEGIN_TEST("walk in order");

  err = walk_test(test_data[0].node, ntree_WALK_IN_ORDER, expected_in_order);
  if (err)
    goto fatal;

  TEST_OK;


  BEGIN_TEST("walk pre order");

  err = walk_test(test_data[0].node, ntree_WALK_PRE_ORDER, expected_pre_order);
  if (err)
    goto fatal;

  TEST_OK;


  BEGIN_TEST("walk post order");

  err = walk_test(test_data[0].node, ntree_WALK_POST_ORDER, expected_post_order);
  if (err)
    goto fatal;

  TEST_OK;


  {
    int height;

    BEGIN_TEST("max height");

    height = ntree_max_height(test_data[0].node);
    printf("height = %d\n", height);
    if (height != 4)
    {
      nfailures++;
      TEST_FAILED;
    }
    else
    {
      TEST_OK;
    }
  }


  {
    int n_nodes;

    BEGIN_TEST("number of nodes");

    n_nodes = ntree_n_nodes(test_data[0].node);
    printf("n_nodes = %d\n", n_nodes);
    if (n_nodes != 10)
    {
      nfailures++;
      TEST_FAILED;
    }
    else
    {
      TEST_OK;
    }
  }


  BEGIN_TEST("copy tree");

  {
    ntree_t *copy;

    BEGIN_SUBTEST("copy");

    err = ntree_copy(test_data[0].node, dup_data, NULL, &copy);
    if (err)
      goto fatal;

    TEST_OK;


    BEGIN_SUBTEST("walk copy in order");

    err = walk_test(copy, ntree_WALK_IN_ORDER, expected_in_order);
    if (err)
      goto fatal;

    TEST_OK;


    BEGIN_SUBTEST("discard copied data");

    err = ntree_walk(copy,
                     ntree_WALK_IN_ORDER | ntree_WALK_ALL,
                     0,
                     free_data,
                     NULL);
    if (err)
      goto fatal;

    TEST_OK;


    BEGIN_SUBTEST("delete tree");

    ntree_delete(copy);

    TEST_OK;
  }

  TEST_OK;


  BEGIN_TEST("delete tree");

  ntree_delete(test_data[0].node);

  TEST_OK;


  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;


fatal:

  if (err)
    printf("Error %x\n", err);

  return err;
}
