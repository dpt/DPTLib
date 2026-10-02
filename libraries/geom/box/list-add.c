/* geom/box/list-add.c -- add a box to a bounded list of boxes */

#include "base/utils.h"

#include "geom/box.h"

/* Merge "b" into "a" in place if they share a complete edge (same width or
 * height, with the other axis touching or overlapping), so the pair can be
 * replaced by a single box covering exactly the same area. */
static int box_merge(box_t *a, const box_t *b)
{
  if (a->y0 == b->y0 && a->y1 == b->y1 && a->x0 <= b->x1 && b->x0 <= a->x1)
  {
    a->x0 = MIN(a->x0, b->x0);
    a->x1 = MAX(a->x1, b->x1);
    return 1;
  }

  if (a->x0 == b->x0 && a->x1 == b->x1 && a->y0 <= b->y1 && b->y0 <= a->y1)
  {
    a->y0 = MIN(a->y0, b->y0);
    a->y1 = MAX(a->y1, b->y1);
    return 1;
  }

  return 0;
}

/* Repeatedly fold any existing entry that "cur" now covers, or that shares a
 * complete edge with it, into "cur" itself; growing "cur" can bring further
 * entries into range, so this settles to a fixed point before "cur" is
 * (re)inserted. Bounded by "max", so an O(n^2) settle is cheap. */
int box_list_add(box_t *list, int *n, int max, const box_t *box)
{
  box_t cur;
  int   changed;

  if (box_is_empty(box))
    return 0;

  cur = *box;

  do
  {
    int i;

    changed = 0;

    for (i = 0; i < *n; i++)
    {
      if (box_contains_box(&cur, &list[i]))
        return 0; /* already covered */

      if (box_contains_box(&list[i], &cur) || box_merge(&cur, &list[i]))
      {
        list[i] = list[--(*n)]; /* absorbed into cur */
        changed = 1;
        break;
      }
    }
  }
  while (changed);

  if (*n < max)
  {
    list[(*n)++] = cur;
    return 0;
  }

  /* full: fold into the last entry rather than growing storage;
   * over-approximates that entry's area but stays correct */
  box_union(&list[max - 1], &cur, &list[max - 1]);

  return 1;
}
