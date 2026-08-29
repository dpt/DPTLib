/* get-bbox.c -- wuss - read a work-area icon's bounding box */

#include "../impl.h"

void wuss_icon_get_bbox(const wuss_icon_t *icon, box_t *bbox)
{
  *bbox = icon->bbox;
}
