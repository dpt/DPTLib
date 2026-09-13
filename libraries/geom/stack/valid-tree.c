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

  for (i = 1; i < n; i++)
    if (items[i].parent < 0 || items[i].parent >= i)
      return 0;

  return 1;
}
