/* wuss/set-colour-fill.c -- give a palette colour a pattern fill */

#include <assert.h>

#include "geom/box.h"

#include "impl.h"

result_t wuss_set_colour_fill(wuss_t            *wuss,
                              wuss_colour_t      colour,
                              const wuss_fill_t *fill)
{
  result_t    rc;
  wuss_fill_t resolved;

  assert(wuss != NULL);
  assert(fill != NULL);

  colour = wuss__resolve_colour(wuss, colour);
  if (colour >= wuss->npalette)
    return result_WUSS_BAD_COLOUR;

  resolved            = *fill;
  resolved.colour     = wuss__resolve_colour(wuss, resolved.colour);
  resolved.pattern_bg = wuss__resolve_colour(wuss, resolved.pattern_bg);

  rc = wuss__validate_fill(wuss, &resolved);
  if (rc != result_OK)
    return rc;

  wuss->fills[colour] = resolved;

  wuss__invalidate_screen(wuss);

  return result_OK;
}
