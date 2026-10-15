/* wuss/set-backdrop.c -- swap the desktop backdrop mid-session */

#include <assert.h>

#include "geom/box.h"

#include "impl.h"

result_t wuss_set_backdrop(wuss_t *wuss, const wuss_backdrop_t *backdrop)
{
  result_t        rc;
  wuss_backdrop_t resolved;

  assert(wuss     != NULL);
  assert(backdrop != NULL);

  resolved            = *backdrop;
  resolved.fill.colour     = wuss__resolve_colour(wuss, resolved.fill.colour);
  resolved.fill.pattern_bg = wuss__resolve_colour(wuss, resolved.fill.pattern_bg);

  rc = wuss__validate_fill(wuss, &resolved.fill);
  if (rc != result_OK)
    return rc;

  wuss->backdrop = resolved;

  /* wuss_COLOUR_BACKDROP now follows the new colour. */
  wuss__rebuild_palettecache(wuss);

  wuss__invalidate_screen(wuss);

  return result_OK;
}
