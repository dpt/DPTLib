/* geom/stack/valid-tree.c -- validate a stack item array is a forest rooted at 0 */

#include "geom/stack.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

int stack__valid_tree(const stack_item_t *items, int n)
{
  int i;

  if (n <= 0)
    return 0;

  if (items[0].parent != -1)
    return 0;

  if (items[0].axis_size == STACK_HUG)
    return 0; /* the root's size comes from the caller, not the tree --
              * see stack_smallest for autosizing the root instead */

  for (i = 1; i < n; i++)
    if (items[i].parent < 0 || items[i].parent >= i)
      return 0;

  for (i = 0; i < n; i++)
  {
    if (items[i].axis_size != STACK_HUG)
      continue;

    if (items[i].kind != stack_KIND_HBOX && items[i].kind != stack_KIND_VBOX)
      return 0; /* only a container has children to hug around */

    if (items[i].flex != 0)
      return 0; /* hug is an absolute size, not something flex can grow */
  }

  return 1;
}
