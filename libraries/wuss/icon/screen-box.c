/* screen-box.c -- wuss - work-area icon bbox to screen space */

#include "../impl.h"

void wuss__icon_screen_box(const wuss_icon_t *icon, box_t *out)
{
  box_t   content;
  point_t scroll;

  wuss__content_box(icon->window, &content);
  scroll = icon->window->scroll;

  out->x0 = content.x0 - scroll.x + icon->bbox.x0;
  out->y0 = content.y0 - scroll.y + icon->bbox.y0;
  out->x1 = content.x0 - scroll.x + icon->bbox.x1;
  out->y1 = content.y0 - scroll.y + icon->bbox.y1;
}
