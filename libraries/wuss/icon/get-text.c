/* get-text.c -- wuss - read a work-area icon's label */

#include "../impl.h"

const char *wuss_icon_get_text(const wuss_icon_t *icon)
{
  return icon->text;
}
