/* wuss/get-font.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

bmfont_t *wuss_get_font(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->fonts[0];
}

bmfont_t *wuss_get_font_n(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss_MAX_FONTS)
    return NULL;

  return wuss->fonts[index];
}
