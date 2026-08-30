/* get-type.c -- wuss - read a work-area icon's type */

#include "../impl.h"

wuss_icon_type_t wuss_icon_get_type(const wuss_icon_t *icon)
{
  return icon->type;
}
