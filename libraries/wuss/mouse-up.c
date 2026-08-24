/* mouse-up.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_up(wuss_t *wuss, int x, int y, wuss_button_t button, wuss_window_t **hit)
{
  wuss_window_t *win;
  box_t           titlebar;

  if (wuss->dragging != NULL)
  {
    win = wuss->dragging;
    if (hit != NULL)
      *hit = win;
    wuss->dragging = NULL;
    return result_OK;
  }

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  wuss__titlebar_box(win, &titlebar);
  if (box_contains_point(&titlebar, x, y))
    return result_OK;

  if (win->client.mouse != NULL)
  {
    int local_x, local_y;

    local_x = x - win->visible.x0;
    local_y = y - win->visible.y0 - wuss__titlebar_height(win);
    return win->client.mouse(win, wuss_MOUSE_UP, local_x, local_y, button, win->client.client_data);
  }

  return result_OK;
}
