/* window-invalidate.c -- wuss - minimal window manager */

#include "impl.h"

void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box)
{
  box_t screen_box, content;

  wuss__content_box(window, &content);

  screen_box.x0 = content.x0 + local_box->x0;
  screen_box.y0 = content.y0 + local_box->y0;
  screen_box.x1 = content.x0 + local_box->x1;
  screen_box.y1 = content.y0 + local_box->y1;

  wuss_invalidate(window->wuss, &screen_box);
}
