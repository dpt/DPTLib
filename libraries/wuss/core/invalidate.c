/* wuss/invalidate.c -- wuss - minimal window manager */

#include <assert.h>

#include "base/utils.h"

#include "impl.h"

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

result_t wuss_invalidate(wuss_t *wuss, const box_t *box)
{
  box_t cur;
  int   changed;

  assert(wuss != NULL);
  assert(box  != NULL);

  if (box_is_empty(box))
    return result_OK;

  cur = *box;

  /* Repeatedly fold any existing entry that "cur" now covers, or that
   * shares a complete edge with it, into "cur" itself; growing "cur" can
   * bring further entries into range, so this settles to a fixed point
   * before "cur" is (re)inserted. Bounded by WUSS_MAX_DIRTY, so an O(n^2)
   * settle is cheap. */
  do
  {
    int i;

    changed = 0;

    for (i = 0; i < wuss->ndirty; i++)
    {
      if (box_contains_box(&cur, &wuss->dirty[i]))
        return result_OK; /* already covered */

      if (box_contains_box(&wuss->dirty[i], &cur) || box_merge(&cur, &wuss->dirty[i]))
      {
        wuss->dirty[i] = wuss->dirty[--wuss->ndirty]; /* absorbed into cur */
        changed = 1;
        break;
      }
    }
  }
  while (changed);

  if (wuss->ndirty < WUSS_MAX_DIRTY)
  {
    wuss->dirty[wuss->ndirty++] = cur;
  }
  else
  {
    /* ponytail: array full, fold into the last entry rather than growing
     * storage; over-approximates that entry's area but stays correct */
    logf_info("wuss_invalidate: %d dirty regions, coalescing "
              "(%d,%d)-(%d,%d) into the last entry",
              WUSS_MAX_DIRTY, cur.x0, cur.y0, cur.x1, cur.y1);
    box_union(&wuss->dirty[WUSS_MAX_DIRTY - 1], &cur, &wuss->dirty[WUSS_MAX_DIRTY - 1]);
  }

  return result_OK;
}

int wuss_get_dirty_count(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->ndirty;
}

void wuss_get_dirty(const wuss_t *wuss, int index, box_t *out)
{
  assert(wuss  != NULL);
  assert(out   != NULL);
  assert(index >= 0 && index < wuss->ndirty);

  *out = wuss->dirty[index];
}
