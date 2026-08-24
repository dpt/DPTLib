/* invalidate.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

result_t wuss_invalidate(wuss_t *wuss, const box_t *box)
{
  assert(wuss != NULL);
  assert(box  != NULL);

  if (box_is_empty(box))
    return result_OK;

  box_union(&wuss->dirty, box, &wuss->dirty);

  return result_OK;
}

void wuss_get_dirty(const wuss_t *wuss, box_t *out)
{
  assert(wuss != NULL);
  assert(out  != NULL);

  *out = wuss->dirty;
}
