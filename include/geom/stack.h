/* geom/stack.h -- box-stack layout solver */

#ifndef GEOM_STACK_H
#define GEOM_STACK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"
#include "geom/inset.h"
#include "geom/size.h"

/* ----------------------------------------------------------------------- */

#define result_STACK_BAD_TREE (result_BASE_STACK + 0)

/** Largest item-array size stack_solve()/stack_smallest() will accept
 * (they use fixed-size internal scratch arrays, not a VLA, for MSVC
 * portability). Ample for any hand-written table. */
#define STACK_MAX_ITEMS 64

/** Special `axis_size` value for a `stack_KIND_HBOX`/`stack_KIND_VBOX` item:
 * `stack_solve` sizes the container's main axis to the sum of its children's
 * main-axis extents (plus gaps and padding) instead of a fixed size or flex.
 * Not valid on the root item, on a `stack_KIND_LEAF`/`stack_KIND_SPACER`, or
 * combined with a non-zero `flex` -- `stack_solve` rejects all three with
 * `result_STACK_BAD_TREE`. */
#define STACK_HUG (-1)

/* ----------------------------------------------------------------------- */

/** The kind of a stack item. */
typedef enum stack_kind
{
  stack_KIND_HBOX,    /**< Stack children left to right.               */
  stack_KIND_VBOX,    /**< Stack children top to bottom.               */
  stack_KIND_SPACER,  /**< Flexible gap; no output box read by anyone. */
  stack_KIND_LEAF      /**< A thing the caller will position.           */
}
stack_kind_t;

/** Cross-axis alignment of a stack item within its container. */
typedef enum stack_align
{
  stack_ALIGN_START,  /**< Against the low cross-axis edge.   */
  stack_ALIGN_CENTRE, /**< Centred on the cross axis.         */
  stack_ALIGN_END,    /**< Against the high cross-axis edge.  */
  stack_ALIGN_FILL     /**< Span the container's cross extent. */
}
stack_align_t;

/** One node in a box-stack tree. */
typedef struct stack_item
{
  stack_kind_t  kind;
  int           parent;     /**< Index of the containing item; -1 for the
                                  root. */

  int           axis_size;  /**< Fixed main-axis extent in px; 0 means "use
                                  flex". */
  int           flex;       /**< Weight for sharing leftover main-axis
                                  space; 0 means "do not grow". */
  int           min;        /**< Main-axis lower clamp in px; 0 means
                                  none. */
  int           max;        /**< Main-axis upper clamp in px; 0 means
                                  unbounded. */

  stack_align_t align;      /**< Cross-axis placement of this item within
                                  its container. */
  int           cross_size; /**< Cross-axis extent in px for
                                  stack_ALIGN_CENTRE/stack_ALIGN_END; 0 means
                                  span the container's full cross extent.
                                  Unused for stack_ALIGN_START/FILL. */

  int           gap;        /**< Containers only: px between adjacent
                                  children. */
  inset_t       pad;        /**< Containers only: inner inset. */
}
stack_item_t;

/* ----------------------------------------------------------------------- */

/* Compact initialisers for a static stack_item_t[] table, covering the
 * fields each kind needs in the common case. Parent is still given
 * explicitly -- a flat initializer list has no nesting to infer it from --
 * but the field names and .kind are no longer spelled out per item. The
 * _EX variants add the fields the plain macro omits, for the occasional
 * item that needs them (e.g. root padding, or a leaf that both flexes and
 * clamps). There is no non-EX/EX split for stack_KIND_SPACER: flex is its
 * only field. */

#define STACK_VBOX(p_, axis_, gap_) \
  { .kind = stack_KIND_VBOX, .parent = (p_), \
    .axis_size = (axis_), .gap = (gap_) }

#define STACK_VBOX_EX(p_, axis_, gap_, padt_, padr_, padb_, padl_) \
  { .kind = stack_KIND_VBOX, .parent = (p_), \
    .axis_size = (axis_), .gap = (gap_), \
    .pad = INSET((padt_), (padr_), (padb_), (padl_)) }

#define STACK_HBOX(p_, axis_, gap_, align_) \
  { .kind = stack_KIND_HBOX, .parent = (p_), \
    .axis_size = (axis_), .gap = (gap_), .align = (align_) }

#define STACK_HBOX_EX(p_, axis_, gap_, align_, padt_, padr_, padb_, padl_) \
  { .kind = stack_KIND_HBOX, .parent = (p_), \
    .axis_size = (axis_), .gap = (gap_), .align = (align_), \
    .pad = INSET((padt_), (padr_), (padb_), (padl_)) }

#define STACK_LEAF(p_, axis_, cross_, align_) \
  { .kind = stack_KIND_LEAF, .parent = (p_), \
    .axis_size = (axis_), .cross_size = (cross_), .align = (align_) }

#define STACK_LEAF_EX(p_, axis_, cross_, align_, flex_, min_, max_) \
  { .kind = stack_KIND_LEAF, .parent = (p_), \
    .axis_size = (axis_), .cross_size = (cross_), .align = (align_), \
    .flex = (flex_), .min = (min_), .max = (max_) }

#define STACK_SPACER(p_, flex_) \
  { .kind = stack_KIND_SPACER, .parent = (p_), .flex = (flex_) }

/**
 * Solve a box-stack tree into boxes.
 *
 * `items[0]` is the root. Every other item names its parent by index, which
 * must appear earlier in the array (parents before children). A container's
 * children are every item naming it as parent, in array order; that order is
 * the stacking order.
 *
 * \param[in]  items Array of stack items describing the tree.
 * \param[in]  n     Number of items in the array.
 * \param[in]  root  The area the root item is laid into.
 * \param[out] out   Array of `n` boxes; `out[i]` receives the computed box
 *                   for `items[i]`, in the same coordinate space as `root`.
 * \return \ref result_OK on success, result_STACK_BAD_TREE if the parent
 *         indices are not a valid parents-before-children forest rooted at
 *         0, or if `n` exceeds \ref STACK_MAX_ITEMS.
 */
result_t stack_solve(const stack_item_t *items,
                     int                 n,
                     const box_t        *root,
                     box_t              *out);

/**
 * Measure the smallest root box the tree can be solved into without any
 * flexible item shrinking below its `min`/`axis_size` -- the size to pass
 * `stack_solve` (and, typically, a window's create call) so every item sits
 * at its minimum rather than being handed extra slack. Each container's
 * minimum main-axis extent is the sum of its children's minima plus gaps and
 * insets; its minimum cross-axis extent is the largest of its children's (a
 * child contributes its `cross_size`, or 0 if unset, since an unset cross
 * size means "span the parent" rather than "needs this much").
 *
 * \param[in]  items Array of stack items describing the tree.
 * \param[in]  n     Number of items in the array.
 * \param[out] out   Set to the root item's minimum size.
 * \return \ref result_OK on success, result_STACK_BAD_TREE if the parent
 *         indices are not a valid parents-before-children forest rooted at
 *         0, or if `n` exceeds \ref STACK_MAX_ITEMS.
 */
result_t stack_smallest(const stack_item_t *items,
                        int                 n,
                        size2d_t           *out);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_STACK_H */
