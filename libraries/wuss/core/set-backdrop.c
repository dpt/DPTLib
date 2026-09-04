/* wuss/set-backdrop.c -- swap the desktop backdrop mid-session */

#include <assert.h>

#include "geom/box.h"

#include "impl.h"

result_t wuss_set_backdrop(wuss_t *wuss, const wuss_backdrop_t *backdrop)
{
  box_t          screen;
  result_t       rc;
  wuss_backdrop_t resolved;

  assert(wuss     != NULL);
  assert(backdrop != NULL);

  resolved            = *backdrop;
  resolved.colour     = wuss__resolve_colour(wuss, resolved.colour);
  resolved.pattern_bg = wuss__resolve_colour(wuss, resolved.pattern_bg);

  rc = wuss__validate_backdrop(wuss, &resolved);
  if (rc != result_OK)
    return rc;

  wuss->backdrop = resolved;

  /* wuss_COLOUR_BACKDROP now follows the new colour. */
  wuss__rebuild_palettecache(wuss);

  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = wuss->scr->size.w;
  screen.y1 = wuss->scr->size.h;
  wuss_invalidate(wuss, &screen);

  return result_OK;
}
