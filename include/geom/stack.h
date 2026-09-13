/* geom/stack.h -- box-stack layout solver */

#ifndef GEOM_STACK_H
#define GEOM_STACK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"

/* ----------------------------------------------------------------------- */

#define result_STACK_BAD_TREE (result_BASE_STACK + 0)

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
  int           parent;  /**< Index of the containing item; -1 for the
                               root. */

  int           size;    /**< Fixed main-axis extent in px; 0 means "use
                               flex". */
  int           flex;    /**< Weight for sharing leftover main-axis space;
                               0 means "do not grow". */
  int           min;     /**< Main-axis lower clamp in px; 0 means none. */
  int           max;     /**< Main-axis upper clamp in px; 0 means
                               unbounded. */

  stack_align_t align;   /**< Cross-axis placement of this item within its
                               container. */

  int           gap;     /**< Containers only: px between adjacent
                               children. */
  int           pad_l;   /**< Containers only: inner inset, left edge.   */
  int           pad_t;   /**< Containers only: inner inset, top edge.    */
  int           pad_r;   /**< Containers only: inner inset, right edge.  */
  int           pad_b;   /**< Containers only: inner inset, bottom edge. */
}
stack_item_t;

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
 *         0.
 */
result_t stack_solve(const stack_item_t *items,
                     int                 n,
                     const box_t        *root,
                     box_t              *out);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_STACK_H */
