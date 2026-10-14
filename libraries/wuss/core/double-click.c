/* wuss/double-click.c -- wuss - double-click detection */

#include "impl.h"

wuss_button_t wuss__double_click(wuss_t        *wuss,
                                 wuss_window_t *hit,
                                 point_t        p,
                                 wuss_button_t  button)
{
  int dx, dy;

  dx = p.x - wuss->last_down.point.x;
  dy = p.y - wuss->last_down.point.y;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;

  if (wuss->last_down.window == hit &&
      wuss->last_down.button == button &&
      (wuss->now_ms - wuss->last_down.ms) <=
        (unsigned int) wuss->input.double_click_ms &&
      dx <= wuss->input.double_click_px &&
      dy <= wuss->input.double_click_px)
  {
    /* Consumed: a third press starts a fresh pair, it does not chain into
     * another double. Reset the button, not the window: NULL is also the
     * backdrop's hit, so a backdrop press would still match. */
    wuss->last_down.button = wuss_BUTTON_NONE;

    return button | wuss_BUTTON_DOUBLE;
  }

  wuss->last_down.window = hit;
  wuss->last_down.button = button;
  wuss->last_down.point  = p;
  wuss->last_down.ms     = wuss->now_ms;

  return button;
}
