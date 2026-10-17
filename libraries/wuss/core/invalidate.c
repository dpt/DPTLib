/* wuss/invalidate.c -- wuss - minimal window manager */

#include <assert.h>

#include "base/utils.h"

#include "impl.h"

/* Shared by wuss_invalidate (wuss->dirty.box) and wuss__touch (wuss->touched.box). */
static void mark_region(box_t       *arr,
                        int         *pn,
                        int          max,
                        const char  *what,
                        const box_t *box)
{
  if (box_list_add(arr, pn, max, box))
    logf_info("wuss_invalidate: %d %s regions, coalescing "
              "(%d,%d)-(%d,%d) into the last entry",
              max, what, box->x0, box->y0, box->x1, box->y1);
}

result_t wuss_invalidate(wuss_t *wuss, const box_t *box)
{
  assert(wuss != NULL);
  assert(box  != NULL);

  mark_region(wuss->dirty.box, &wuss->dirty.n, WUSS_MAX_DIRTY, "dirty", box);

  return result_OK;
}

/* Mark a screen-space region as having changed pixels without needing a
 * repaint -- e.g. wuss__blit_pieces sliding a window's own content to a new
 * position: the backing bitmap is already correct there, but a frontend that
 * only re-uploads what wuss_get_touched_extent reports still needs to know
 * the pixels moved. Never folded into wuss->dirty.box, so wuss_redraw_dirty
 * never repaints it. */
void wuss__touch(wuss_t *wuss, const box_t *box)
{
  assert(wuss != NULL);
  assert(box  != NULL);

  mark_region(wuss->touched.box, &wuss->touched.n, WUSS_MAX_DIRTY, "touched", box);
}

void wuss__screen_box(const wuss_t *wuss, box_t *out)
{
  *out = (box_t) BOX_POS_SIZE(0, 0, wuss->scr->size.w, wuss->scr->size.h);
}

void wuss__invalidate_screen(wuss_t *wuss)
{
  box_t screen;

  wuss__screen_box(wuss, &screen);
  wuss_invalidate(wuss, &screen);
}

int wuss_get_dirty_count(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->dirty.n;
}

void wuss_get_dirty(const wuss_t *wuss, int index, box_t *out)
{
  assert(wuss  != NULL);
  assert(out   != NULL);
  assert(index >= 0 && index < wuss->dirty.n);

  *out = wuss->dirty.box[index];
}

int wuss_get_touched_extent(const wuss_t *wuss, box_t *out)
{
  box_t u;
  int   i;

  assert(wuss != NULL);
  assert(out  != NULL);

  if (wuss->touched.n == 0)
    return 0;

  u = wuss->touched.box[0];
  for (i = 1; i < wuss->touched.n; i++)
    box_union(&u, &wuss->touched.box[i], &u);

  *out = u;

  return 1;
}

void wuss_clear_touched(wuss_t *wuss)
{
  assert(wuss != NULL);

  wuss->touched.n = 0;
}
