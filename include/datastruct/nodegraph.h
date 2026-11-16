/* datastruct/nodegraph.h -- dataflow graph of nodes joined by typed links */

/**
 * \file nodegraph.h
 *
 * A dataflow graph: nodes joined by typed links, evaluated in dependency
 * order.
 *
 * Each node is an instance of a client-defined class, which names the types
 * of its input ports and its single output port and supplies the evaluate
 * callback. Payloads are opaque: the client owns every buffer, the graph
 * only passes pointers from one node's output to the inputs of others.
 *
 * An output may feed any number of inputs. An input takes at most one link;
 * linking an occupied input replaces its link. Links that would form a cycle
 * are refused, so the graph is always a DAG.
 *
 * Each node caches its last output. Changing a node, or its links, marks it
 * and everything downstream dirty; nodegraph_evaluate recomputes only dirty
 * nodes.
 *
 * Nodes are named by integer handles which are never reused within a graph.
 */

#ifndef DATASTRUCT_NODEGRAPH_H
#define DATASTRUCT_NODEGRAPH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

/* ----------------------------------------------------------------------- */

#define result_NODEGRAPH_CYCLE (result_BASE_NODEGRAPH + 0)

/* ----------------------------------------------------------------------- */

#define T nodegraph_t

/**
 * A node graph's type.
 */
typedef struct nodegraph T;

/**
 * A node's handle.
 */
typedef int nodegraph_node_t;

/**
 * The handle of no node.
 */
#define nodegraph_NONE (-1)

/**
 * The most input ports a node class may have.
 */
#define nodegraph_MAX_INPUTS 4

/**
 * The output_type of a sink: a class with no output port, which nothing can
 * link from. Client port types must not use this value.
 */
#define nodegraph_NO_OUTPUT (-1)

/**
 * Evaluate a node.
 *
 * \param state  The node's client state.
 * \param inputs The upstream outputs, one per input port. NULL where the
 *               port is not linked.
 * \param output Set to the node's output payload on success.
 *
 * \return Error indication. A failure stops nodegraph_evaluate and leaves
 *         the node dirty.
 */
typedef result_t (nodegraph_evaluate_t)(void       *state,
                                        void *const inputs[],
                                        void      **output);

/**
 * Destroy a node's client state.
 *
 * \param state The node's client state.
 */
typedef void (nodegraph_destroy_t)(void *state);

/**
 * A node class, defined by the client. Port types are client-chosen ids; a
 * link joins only equal types. ninputs is at most nodegraph_MAX_INPUTS.
 * output_type is nodegraph_NO_OUTPUT for a sink. destroy may be NULL.
 */
typedef struct nodegraph_class
{
  const char           *name;
  int                   ninputs;
  int                   input_types[nodegraph_MAX_INPUTS];
  int                   output_type;
  nodegraph_evaluate_t *evaluate;
  nodegraph_destroy_t  *destroy;
}
nodegraph_class_t;

/* ----------------------------------------------------------------------- */

/**
 * Create an empty node graph.
 *
 * \param[out] new_graph Created graph.
 *
 * \return Error indication.
 */
result_t nodegraph_create(T **new_graph);

/**
 * Destroy a node graph, calling each node's destroy hook.
 *
 * \param doomed Graph to destroy. May be NULL.
 */
void nodegraph_destroy(T *doomed);

/* ----------------------------------------------------------------------- */

/**
 * Add a node. It starts dirty and unlinked.
 *
 * \param graph     Graph.
 * \param cls       Node's class. Must outlive the node.
 * \param state     Node's client state, passed to the class's callbacks.
 * \param x         Node's x position.
 * \param y         Node's y position.
 * \param[out] node New node's handle.
 *
 * \return Error indication.
 */
result_t nodegraph_add(T                       *graph,
                       const nodegraph_class_t *cls,
                       void                    *state,
                       int                      x,
                       int                      y,
                       nodegraph_node_t        *node);

/**
 * Remove a node, unlinking it and calling its destroy hook. Nodes it fed are
 * marked dirty.
 *
 * \param graph Graph.
 * \param node  Node to remove.
 *
 * \return result_NOT_FOUND if node is not in the graph.
 */
result_t nodegraph_remove(T *graph, nodegraph_node_t node);

/**
 * Link one node's output to another's input, replacing any link already on
 * that input. Marks the destination dirty.
 *
 * \param graph Graph.
 * \param from  Source node.
 * \param to    Destination node.
 * \param input Destination input port.
 *
 * \return result_NOT_FOUND for an unknown node, result_BAD_ARG for a bad
 *         port, result_INCOMPATIBLE when the types differ (or from is a
 *         sink) or result_NODEGRAPH_CYCLE when the link would form a cycle.
 */
result_t nodegraph_link(T               *graph,
                        nodegraph_node_t from,
                        nodegraph_node_t to,
                        int              input);

/**
 * Remove the link on an input, if any. Marks the node dirty if it had one.
 *
 * \param graph Graph.
 * \param to    Destination node.
 * \param input Destination input port.
 *
 * \return result_NOT_FOUND for an unknown node or result_BAD_ARG for a bad
 *         port.
 */
result_t nodegraph_unlink(T *graph, nodegraph_node_t to, int input);

/**
 * Return the node linked to an input.
 *
 * \param graph Graph.
 * \param to    Destination node.
 * \param input Destination input port.
 *
 * \return Source node, or nodegraph_NONE if unlinked or invalid.
 */
nodegraph_node_t nodegraph_source(const T         *graph,
                                  nodegraph_node_t to,
                                  int              input);

/* ----------------------------------------------------------------------- */

/**
 * Mark a node and everything downstream of it dirty, e.g. after the client
 * changes its state.
 *
 * \param graph Graph.
 * \param node  Node.
 */
void nodegraph_dirty(T *graph, nodegraph_node_t node);

/**
 * Evaluate every dirty node, inputs before the nodes they feed.
 *
 * \param graph Graph.
 *
 * \return The first evaluate callback failure, else result_OK.
 */
result_t nodegraph_evaluate(T *graph);

/**
 * Return a node's last output.
 *
 * \param graph Graph.
 * \param node  Node.
 *
 * \return Output payload, or NULL if never evaluated or invalid.
 */
void *nodegraph_output(const T *graph, nodegraph_node_t node);

/* ----------------------------------------------------------------------- */

/**
 * Step through the graph's nodes in handle order.
 *
 * \param graph Graph.
 * \param node  Previous node, or nodegraph_NONE to start.
 *
 * \return Next node, or nodegraph_NONE at the end.
 */
nodegraph_node_t nodegraph_next(const T *graph, nodegraph_node_t node);

/**
 * Return a node's class.
 *
 * \param graph Graph.
 * \param node  Node.
 *
 * \return Class, or NULL if invalid.
 */
const nodegraph_class_t *nodegraph_class(const T         *graph,
                                         nodegraph_node_t node);

/**
 * Return a node's client state.
 *
 * \param graph Graph.
 * \param node  Node.
 *
 * \return State, or NULL if invalid.
 */
void *nodegraph_state(const T *graph, nodegraph_node_t node);

/**
 * Read a node's position.
 *
 * \param graph  Graph.
 * \param node   Node.
 * \param[out] x Node's x position.
 * \param[out] y Node's y position.
 *
 * \return result_NOT_FOUND if node is not in the graph.
 */
result_t nodegraph_get_pos(const T         *graph,
                           nodegraph_node_t node,
                           int             *x,
                           int             *y);

/**
 * Move a node. Position does not affect evaluation.
 *
 * \param graph Graph.
 * \param node  Node.
 * \param x     New x position.
 * \param y     New y position.
 *
 * \return result_NOT_FOUND if node is not in the graph.
 */
result_t nodegraph_set_pos(T *graph, nodegraph_node_t node, int x, int y);

/* ----------------------------------------------------------------------- */

#undef T

#ifdef __cplusplus
}
#endif

#endif /* DATASTRUCT_NODEGRAPH_H */
