/* hit-test.c -- wuss - minimal window manager */

#include "../impl.h"

wuss_furniture_region_t wuss__furniture_hit_test(const wuss_window_t *window,
                                                 int                  x,
                                                 int                  y)
{
  box_t box;

  if (!(window->flags & wuss_WINDOW_NO_TITLEBAR))
  {
    if (!(window->flags & wuss_WINDOW_NO_CLOSE))
    {
      wuss__close_box(window, &box);
      if (box_contains_point(&box, x, y))
        return wuss_FURNITURE_CLOSE;
    }

    wuss__titlebar_box(window, &box);
    if (box_contains_point(&box, x, y))
      return wuss_FURNITURE_TITLE;
  }

  return wuss_FURNITURE_CONTENT;
}
