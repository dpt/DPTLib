/* wuss/icon/get-selected.c -- query a radio/option icon's latched state */

#include <assert.h>

#include "../impl.h"

int wuss_icon_get_selected(const wuss_icon_t *icon)
{
  assert(icon != NULL);

  return icon->selected;
}
