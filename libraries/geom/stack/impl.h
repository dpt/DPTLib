/* geom/stack/impl.h -- box-stack layout solver */

#ifndef IMPL_H
#define IMPL_H

#include "geom/stack.h"

/**
 * Affirms if `items[0..n)` form a valid parents-before-children forest
 * rooted at index 0.
 *
 * \param[in] items Array of stack items.
 * \param[in] n     Number of items in the array.
 * \return Non-zero if the tree is valid.
 */
int stack__valid_tree(const stack_item_t *items, int n);

/**
 * The main-axis extent a leaf or spacer is given: its own `axis_size`, or the
 * largest `axis_size` in its size group if it has one.
 *
 * \param[in] items Array of stack items.
 * \param[in] n     Number of items in the array.
 * \param[in] index Item to query.
 * \return Extent in px.
 */
int stack__axis_size(const stack_item_t *items, int n, int index);

#endif /* IMPL_H */
