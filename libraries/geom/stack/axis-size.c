/* geom/stack/axis-size.c -- effective main-axis size of a leaf or spacer */

#include "base/utils.h"

#include "geom/stack.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

int stack__axis_size(const stack_item_t *items, int n, int index)
{
  int size;
  int i;

  size = items[index].axis_size;

  if (items[index].group == 0)
    return size;

  for (i = 0; i < n; i++)
    if (items[i].group == items[index].group)
      size = MAX(size, items[i].axis_size);

  return size;
}
