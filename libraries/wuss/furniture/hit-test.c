/* hit-test.c -- wuss - minimal window manager */

#include "../impl.h"

wuss_furniture_region_t wuss__furniture_hit_test(const wuss_window_t *window,
                                                 point_t              p)
{
  box_t box;

  if (!(window->flags & wuss_WINDOW_NO_TITLEBAR))
  {
    if (!(window->flags & wuss_WINDOW_NO_BACK))
    {
      wuss__back_box(window, &box);
      if (box_contains_point(&box, p.x, p.y))
        return wuss_FURNITURE_BACK;
    }

    if (!(window->flags & wuss_WINDOW_NO_CLOSE))
    {
      wuss__close_box(window, &box);
      if (box_contains_point(&box, p.x, p.y))
        return wuss_FURNITURE_CLOSE;
    }

    if (!(window->flags & wuss_WINDOW_NO_TOGGLE_SIZE))
    {
      wuss__toggle_box(window, &box);
      if (box_contains_point(&box, p.x, p.y))
        return wuss_FURNITURE_TOGGLE_SIZE;
    }

    wuss__titlebar_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_TITLE;
  }

  if (!(window->flags & wuss_WINDOW_NO_RESIZE))
  {
    wuss__resize_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_RESIZE;
  }

  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
  {
    wuss__vscroll_up_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_VSCROLL_UP;

    wuss__vscroll_down_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_VSCROLL_DOWN;

    wuss__vscroll_well_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_VSCROLL_WELL;
  }

  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    wuss__hscroll_left_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_HSCROLL_LEFT;

    wuss__hscroll_right_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_HSCROLL_RIGHT;

    wuss__hscroll_well_box(window, &box);
    if (box_contains_point(&box, p.x, p.y))
      return wuss_FURNITURE_HSCROLL_WELL;
  }

  return wuss_FURNITURE_CONTENT;
}
