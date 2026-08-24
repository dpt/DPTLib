/* mouse-down.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_down(wuss_t *wuss, int x, int y, wuss_button_t button, wuss_window_t **hit)
{
  wuss_window_t *win;
  box_t           titlebar;

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  wuss__titlebar_box(win, &titlebar);
  if (box_contains_point(&titlebar, x, y))
  {
    if (button == wuss_BUTTON_SELECT)
      wuss_window_bring_to_front(win);

    wuss->dragging = win;
    wuss->drag_dx  = x - win->visible.x0;
    wuss->drag_dy  = y - win->visible.y0;
    return result_OK;
  }

  if (win->client.mouse != NULL)
  {
    int local_x, local_y;

    local_x = x - win->visible.x0;
    local_y = y - win->visible.y0 - wuss->titlebar_height;
    return win->client.mouse(win, wuss_MOUSE_DOWN, local_x, local_y, button, win->client.client_data);
  }

  return result_OK;
}
