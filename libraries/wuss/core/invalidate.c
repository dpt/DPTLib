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

/* Shared by wuss_invalidate (wuss->dirty) and wuss__touch (wuss->touched):
 * repeatedly fold any existing entry that "cur" now covers, or that shares a
 * complete edge with it, into "cur" itself; growing "cur" can bring further
 * entries into range, so this settles to a fixed point before "cur" is
 * (re)inserted. Bounded by "max", so an O(n^2) settle is cheap. Returns 1 if
 * "box" was already fully covered (nothing to do). */
static int mark_region(box_t       *arr,
                       int         *pn,
                       int          max,
                       const char  *what,
                       const box_t *box)
{
  box_t cur;
  int   changed;

  if (box_is_empty(box))
    return 1;

  cur = *box;

  do
  {
    int i;

    changed = 0;

    for (i = 0; i < *pn; i++)
    {
      if (box_contains_box(&cur, &arr[i]))
        return 1; /* already covered */

      if (box_contains_box(&arr[i], &cur) || box_merge(&cur, &arr[i]))
      {
        arr[i] = arr[--(*pn)]; /* absorbed into cur */
        changed = 1;
        break;
      }
    }
  }
  while (changed);

  if (*pn < max)
  {
    arr[(*pn)++] = cur;
  }
  else
  {
    /* ponytail: array full, fold into the last entry rather than growing
     * storage; over-approximates that entry's area but stays correct */
    logf_info("wuss_invalidate: %d %s regions, coalescing "
              "(%d,%d)-(%d,%d) into the last entry",
              max, what, cur.x0, cur.y0, cur.x1, cur.y1);
    box_union(&arr[max - 1], &cur, &arr[max - 1]);
  }

  return 0;
}

result_t wuss_invalidate(wuss_t *wuss, const box_t *box)
{
  assert(wuss != NULL);
  assert(box  != NULL);

  mark_region(wuss->dirty, &wuss->ndirty, WUSS_MAX_DIRTY, "dirty", box);

  return result_OK;
}

/* Mark a screen-space region as having changed pixels without needing a
 * repaint -- e.g. wuss__blit_pieces sliding a window's own content to a new
 * position: the backing bitmap is already correct there, but a frontend that
 * only re-uploads what wuss_get_touched_extent reports still needs to know
 * the pixels moved. Never folded into wuss->dirty, so wuss_redraw_dirty
 * never repaints it. */
void wuss__touch(wuss_t *wuss, const box_t *box)
{
  assert(wuss != NULL);
  assert(box  != NULL);

  mark_region(wuss->touched, &wuss->ntouched, WUSS_MAX_DIRTY, "touched", box);
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

int wuss_get_touched_extent(const wuss_t *wuss, box_t *out)
{
  box_t u;
  int   i;

  assert(wuss != NULL);
  assert(out  != NULL);

  if (wuss->ntouched == 0)
    return 0;

  u = wuss->touched[0];
  for (i = 1; i < wuss->ntouched; i++)
    box_union(&u, &wuss->touched[i], &u);

  *out = u;

  return 1;
}

void wuss_clear_touched(wuss_t *wuss)
{
  assert(wuss != NULL);

  wuss->ntouched = 0;
}
