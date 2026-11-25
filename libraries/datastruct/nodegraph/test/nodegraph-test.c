/* datastruct/nodegraph/test/nodegraph-test.c */

#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "datastruct/nodegraph.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

enum { TYPE_NUM, TYPE_OTHER };

typedef struct num
{
  int value; /* constant, or added to the sum of inputs */
  int out;
  int evals;
  int destroyed;
}
num_t;

static result_t num_evaluate(void       *state,
                             void *const inputs[],
                             void      **output)
{
  num_t *n;
  int    i;

  n      = state;
  n->out = n->value;
  for (i = 0; i < 2; i++)
    if (inputs[i])
      n->out += *(int *) inputs[i];
  n->evals++;
  *output = &n->out;
  return result_OK;
}

static void num_destroy(void *state)
{
  ((num_t *) state)->destroyed++;
}

static const nodegraph_class_t sum_class =
{
  "Sum", 2, { TYPE_NUM, TYPE_NUM }, TYPE_NUM, num_evaluate, num_destroy
};

static const nodegraph_class_t other_class =
{
  "Other", 2, { TYPE_OTHER, TYPE_OTHER }, TYPE_OTHER, num_evaluate, NULL
};

static const nodegraph_class_t sink_class =
{
  "Sink", 1, { nodegraph_NO_OUTPUT }, nodegraph_NO_OUTPUT, num_evaluate, NULL
};

#define CHECK(cond)                                         \
  do                                                        \
  {                                                         \
    if (!(cond))                                            \
    {                                                       \
      printf("nodegraph: line %d: %s\n", __LINE__, #cond);  \
      goto failure;                                         \
    }                                                       \
  }                                                         \
  while (0)

static int outof(nodegraph_t *g, nodegraph_node_t n)
{
  return *(int *) nodegraph_output(g, n);
}

/* ----------------------------------------------------------------------- */

result_t nodegraph_test(const char *resources)
{
  result_t         rc;
  nodegraph_t     *g;
  num_t            nums[5] = { { 1 }, { 10 }, { 100 }, { 1000 }, { 7 } };
  nodegraph_node_t a, b, c, d, o;
  int              x, y;
  int              i;

  NOT_USED(resources);

  g = NULL;

  rc = nodegraph_create(NULL, &g);
  CHECK(rc == result_OK);

  /* a and b feed c; c feeds d's both inputs (fan-out) */
  CHECK(nodegraph_add(g, &sum_class, &nums[0], 0, 0, &a) == result_OK);
  CHECK(nodegraph_add(g, &sum_class, &nums[1], 0, 0, &b) == result_OK);
  CHECK(nodegraph_add(g, &sum_class, &nums[2], 0, 0, &c) == result_OK);
  CHECK(nodegraph_add(g, &sum_class, &nums[3], 5, 6, &d) == result_OK);
  CHECK(nodegraph_add(g, &other_class, &nums[4], 0, 0, &o) == result_OK);

  /* d first so evaluation must reach upstream before it */
  CHECK(nodegraph_link(g, c, d, 0) == result_OK);
  CHECK(nodegraph_link(g, c, d, 1) == result_OK);
  CHECK(nodegraph_link(g, a, c, 0) == result_OK);
  CHECK(nodegraph_link(g, b, c, 1) == result_OK);

  /* refusals */
  CHECK(nodegraph_link(g, d, a, 0) == result_NODEGRAPH_CYCLE);
  CHECK(nodegraph_link(g, c, c, 0) == result_NODEGRAPH_CYCLE);
  CHECK(nodegraph_link(g, a, o, 0) == result_INCOMPATIBLE);
  CHECK(nodegraph_link(g, a, c, 2) == result_BAD_ARG);
  CHECK(nodegraph_link(g, a, 99, 0) == result_NOT_FOUND);
  CHECK(nodegraph_source(g, d, 0) == c);
  CHECK(nodegraph_source(g, a, 0) == nodegraph_NONE);

  /* unconnected inputs arrive as NULL */
  CHECK(nodegraph_evaluate(g) == result_OK);
  CHECK(outof(g, c) == 111);
  CHECK(outof(g, d) == 1000 + 2 * 111);
  for (i = 0; i < 5; i++)
    CHECK(nums[i].evals == 1);

  /* clean graph: nothing recomputes */
  CHECK(nodegraph_evaluate(g) == result_OK);
  CHECK(nums[3].evals == 1);

  /* dirtying b recomputes b, c and d but not a */
  nums[1].value = 20;
  nodegraph_dirty(g, b);
  CHECK(nodegraph_evaluate(g) == result_OK);
  CHECK(nums[0].evals == 1);
  CHECK(nums[1].evals == 2 && nums[2].evals == 2 && nums[3].evals == 2);
  CHECK(outof(g, d) == 1000 + 2 * 121);

  /* relinking an occupied input replaces its link */
  CHECK(nodegraph_link(g, a, c, 1) == result_OK);
  CHECK(nodegraph_source(g, c, 1) == a);
  CHECK(nodegraph_evaluate(g) == result_OK);
  CHECK(outof(g, c) == 102);
  CHECK(nums[0].evals == 1 && nums[2].evals == 3);

  /* unlinking */
  CHECK(nodegraph_unlink(g, d, 1) == result_OK);
  CHECK(nodegraph_evaluate(g) == result_OK);
  CHECK(outof(g, d) == 1102);

  /* removal unlinks, dirties downstream, calls destroy, leaves a hole */
  CHECK(nodegraph_remove(g, a) == result_OK);
  CHECK(nums[0].destroyed == 1);
  CHECK(nodegraph_source(g, c, 0) == nodegraph_NONE);
  CHECK(nodegraph_class(g, a) == NULL);
  CHECK(nodegraph_remove(g, a) == result_NOT_FOUND);
  CHECK(nodegraph_evaluate(g) == result_OK);
  CHECK(outof(g, c) == 100 && outof(g, d) == 1100);
  CHECK(nodegraph_next(g, nodegraph_NONE) == b);

  /* handles are not reused */
  CHECK(nodegraph_add(g, &sum_class, &nums[0], 0, 0, &a) == result_OK);
  CHECK(a == 5);

  CHECK(nodegraph_get_pos(g, d, &x, &y) == result_OK && x == 5 && y == 6);
  CHECK(nodegraph_set_pos(g, d, 8, 9) == result_OK);
  CHECK(nodegraph_get_pos(g, d, &x, &y) == result_OK && x == 8 && y == 9);
  CHECK(nodegraph_state(g, d) == &nums[3]);

  /* nothing links from a sink, even into a port typed as its output */
  CHECK(nodegraph_add(g, &sink_class, &nums[4], 0, 0, &o) == result_OK);
  CHECK(nodegraph_link(g, o, o, 0) == result_INCOMPATIBLE);

  nodegraph_destroy(g);
  g = NULL;
  CHECK(nums[0].destroyed == 2 && nums[1].destroyed == 1);

  return result_TEST_PASSED;


failure:
  nodegraph_destroy(g);
  return result_TEST_FAILED;
}
