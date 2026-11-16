/* datastruct/nodegraph/nodegraph.c -- dataflow graph of nodes joined by typed links */

#include <stdlib.h>

#include "base/result.h"
#include "utils/array.h"

#include "datastruct/nodegraph.h"

/* ----------------------------------------------------------------------- */

/* Nodes live in an array indexed by handle. Removed nodes leave a hole (cls
 * is NULL) so handles are never reused.
 * ponytail: holes are never reclaimed and downstream marking scans every
 * node; fine for hand-built graphs of tens of nodes. */

typedef struct nodegraph_slot
{
  const nodegraph_class_t *cls; /* NULL if removed */
  void                    *state;
  void                    *output;
  nodegraph_node_t         inputs[nodegraph_MAX_INPUTS];
  int                      x, y;
  int                      dirty;
}
nodegraph_slot_t;

struct nodegraph
{
  nodegraph_slot_t *slots;
  int               used;
  int               allocated;
};

/* ----------------------------------------------------------------------- */

static nodegraph_slot_t *nodegraph_get(const nodegraph_t *graph,
                                       nodegraph_node_t   node)
{
  if (node < 0 || node >= graph->used || graph->slots[node].cls == NULL)
    return NULL;
  return &graph->slots[node];
}

/* Return non-zero if 'target' is 'node' or lies upstream of it. */
static int nodegraph_upstream(const nodegraph_t *graph,
                              nodegraph_node_t   node,
                              nodegraph_node_t   target)
{
  const nodegraph_slot_t *slot;
  int                     i;

  if (node == target)
    return 1;
  slot = &graph->slots[node];
  for (i = 0; i < slot->cls->ninputs; i++)
    if (slot->inputs[i] != nodegraph_NONE &&
        nodegraph_upstream(graph, slot->inputs[i], target))
      return 1;
  return 0;
}

static result_t nodegraph_eval(nodegraph_t *graph, nodegraph_node_t node)
{
  result_t          rc;
  nodegraph_slot_t *slot;
  void             *inputs[nodegraph_MAX_INPUTS];
  void             *output;
  int               i;

  slot = &graph->slots[node];
  if (!slot->dirty)
    return result_OK;

  for (i = 0; i < slot->cls->ninputs; i++)
  {
    inputs[i] = NULL;
    if (slot->inputs[i] == nodegraph_NONE)
      continue;
    rc = nodegraph_eval(graph, slot->inputs[i]);
    if (rc)
      return rc;
    inputs[i] = graph->slots[slot->inputs[i]].output;
  }

  output = NULL;
  rc = slot->cls->evaluate(slot->state, inputs, &output);
  if (rc)
    return rc;

  slot->output = output;
  slot->dirty  = 0;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t nodegraph_create(nodegraph_t **new_graph)
{
  nodegraph_t *graph;

  *new_graph = NULL;

  graph = calloc(1, sizeof(*graph));
  if (graph == NULL)
    return result_OOM;

  *new_graph = graph;
  return result_OK;
}

void nodegraph_destroy(nodegraph_t *doomed)
{
  nodegraph_slot_t *slot;
  int               i;

  if (doomed == NULL)
    return;

  for (i = 0; i < doomed->used; i++)
  {
    slot = &doomed->slots[i];
    if (slot->cls && slot->cls->destroy)
      slot->cls->destroy(slot->state);
  }
  free(doomed->slots);
  free(doomed);
}

/* ----------------------------------------------------------------------- */

result_t nodegraph_add(nodegraph_t             *graph,
                       const nodegraph_class_t *cls,
                       void                    *state,
                       int                      x,
                       int                      y,
                       nodegraph_node_t        *node)
{
  nodegraph_slot_t *slot;
  int               i;

  *node = nodegraph_NONE;

  if (cls->ninputs < 0 || cls->ninputs > nodegraph_MAX_INPUTS)
    return result_BAD_ARG;

  if (array_grow((void **) &graph->slots, sizeof(*graph->slots),
                 graph->used, &graph->allocated, 1, 8))
    return result_OOM;

  slot         = &graph->slots[graph->used];
  slot->cls    = cls;
  slot->state  = state;
  slot->output = NULL;
  for (i = 0; i < nodegraph_MAX_INPUTS; i++)
    slot->inputs[i] = nodegraph_NONE;
  slot->x      = x;
  slot->y      = y;
  slot->dirty  = 1;

  *node = graph->used++;
  return result_OK;
}

result_t nodegraph_remove(nodegraph_t *graph, nodegraph_node_t node)
{
  nodegraph_slot_t *slot;
  nodegraph_slot_t *other;
  int               i, j;

  slot = nodegraph_get(graph, node);
  if (slot == NULL)
    return result_NOT_FOUND;

  nodegraph_dirty(graph, node);

  for (i = 0; i < graph->used; i++)
  {
    other = &graph->slots[i];
    if (other->cls == NULL)
      continue;
    for (j = 0; j < other->cls->ninputs; j++)
      if (other->inputs[j] == node)
        other->inputs[j] = nodegraph_NONE;
  }

  if (slot->cls->destroy)
    slot->cls->destroy(slot->state);
  slot->cls    = NULL;
  slot->state  = NULL;
  slot->output = NULL;
  return result_OK;
}

result_t nodegraph_link(nodegraph_t     *graph,
                        nodegraph_node_t from,
                        nodegraph_node_t to,
                        int              input)
{
  nodegraph_slot_t *src;
  nodegraph_slot_t *dst;

  src = nodegraph_get(graph, from);
  dst = nodegraph_get(graph, to);
  if (src == NULL || dst == NULL)
    return result_NOT_FOUND;
  if (input < 0 || input >= dst->cls->ninputs)
    return result_BAD_ARG;
  if (src->cls->output_type != dst->cls->input_types[input])
    return result_INCOMPATIBLE;
  if (nodegraph_upstream(graph, from, to))
    return result_NODEGRAPH_CYCLE;

  dst->inputs[input] = from;
  nodegraph_dirty(graph, to);
  return result_OK;
}

result_t nodegraph_unlink(nodegraph_t *graph, nodegraph_node_t to, int input)
{
  nodegraph_slot_t *dst;

  dst = nodegraph_get(graph, to);
  if (dst == NULL)
    return result_NOT_FOUND;
  if (input < 0 || input >= dst->cls->ninputs)
    return result_BAD_ARG;

  if (dst->inputs[input] != nodegraph_NONE)
  {
    dst->inputs[input] = nodegraph_NONE;
    nodegraph_dirty(graph, to);
  }
  return result_OK;
}

nodegraph_node_t nodegraph_source(const nodegraph_t *graph,
                                  nodegraph_node_t   to,
                                  int                input)
{
  const nodegraph_slot_t *dst;

  dst = nodegraph_get(graph, to);
  if (dst == NULL || input < 0 || input >= dst->cls->ninputs)
    return nodegraph_NONE;
  return dst->inputs[input];
}

/* ----------------------------------------------------------------------- */

/* A dirty node's downstream is always dirty too, so marking can stop at any
 * node already dirty. */
void nodegraph_dirty(nodegraph_t *graph, nodegraph_node_t node)
{
  nodegraph_slot_t *slot;
  nodegraph_slot_t *other;
  int               i, j;

  slot = nodegraph_get(graph, node);
  if (slot == NULL)
    return;

  slot->dirty = 1;

  for (i = 0; i < graph->used; i++)
  {
    other = &graph->slots[i];
    if (other->cls == NULL || other->dirty)
      continue;
    for (j = 0; j < other->cls->ninputs; j++)
      if (other->inputs[j] == node)
      {
        nodegraph_dirty(graph, i);
        break;
      }
  }
}

result_t nodegraph_evaluate(nodegraph_t *graph)
{
  result_t rc;
  int      i;

  for (i = 0; i < graph->used; i++)
  {
    if (graph->slots[i].cls == NULL)
      continue;
    rc = nodegraph_eval(graph, i);
    if (rc)
      return rc;
  }
  return result_OK;
}

void *nodegraph_output(const nodegraph_t *graph, nodegraph_node_t node)
{
  const nodegraph_slot_t *slot;

  slot = nodegraph_get(graph, node);
  return slot ? slot->output : NULL;
}

/* ----------------------------------------------------------------------- */

nodegraph_node_t nodegraph_next(const nodegraph_t *graph,
                                nodegraph_node_t   node)
{
  for (node = (node < 0) ? 0 : node + 1; node < graph->used; node++)
    if (graph->slots[node].cls)
      return node;
  return nodegraph_NONE;
}

const nodegraph_class_t *nodegraph_class(const nodegraph_t *graph,
                                         nodegraph_node_t   node)
{
  const nodegraph_slot_t *slot;

  slot = nodegraph_get(graph, node);
  return slot ? slot->cls : NULL;
}

void *nodegraph_state(const nodegraph_t *graph, nodegraph_node_t node)
{
  const nodegraph_slot_t *slot;

  slot = nodegraph_get(graph, node);
  return slot ? slot->state : NULL;
}

result_t nodegraph_get_pos(const nodegraph_t *graph,
                           nodegraph_node_t   node,
                           int               *x,
                           int               *y)
{
  const nodegraph_slot_t *slot;

  slot = nodegraph_get(graph, node);
  if (slot == NULL)
    return result_NOT_FOUND;
  *x = slot->x;
  *y = slot->y;
  return result_OK;
}

result_t nodegraph_set_pos(nodegraph_t     *graph,
                           nodegraph_node_t node,
                           int              x,
                           int              y)
{
  nodegraph_slot_t *slot;

  slot = nodegraph_get(graph, node);
  if (slot == NULL)
    return result_NOT_FOUND;
  slot->x = x;
  slot->y = y;
  return result_OK;
}
