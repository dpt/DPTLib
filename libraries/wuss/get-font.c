/* wuss/get-font.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

bmfont_t *wuss_get_font(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->font;
}
