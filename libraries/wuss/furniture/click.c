/* wuss/furniture/click.c -- wuss - pointer clicks on window furniture */

#include "../core/types.h"
#include "../core/dispatch.h"
#include "../core/geometry.h"
#include "../core/invalidate.h"
#include "press.h"

void wuss__furniture_set_pressed(wuss_window_t          *win,
                                 wuss_furniture_region_t region)
{
  wuss_t *wuss;
  box_t   pressed;

  wuss = win->wuss;

  if (wuss->furniture.pressed_region != wuss_FURNITURE_NONE)
  {
    wuss__furniture_pressed_box(win, wuss->furniture.pressed_region, &pressed);
    wuss__invalidate_clipped(win, &pressed);
  }

  wuss->furniture.pressed_region = region;

  if (region != wuss_FURNITURE_NONE)
  {
    wuss__furniture_pressed_box(win, region, &pressed);
    wuss__invalidate_clipped(win, &pressed);
  }
}

void wuss__furniture_hold(wuss_window_t          *win,
                          wuss_furniture_region_t region)
{
  win->wuss->furniture.dragging    = win;
  win->wuss->furniture.drag_kind   = wuss_FURNITURE_DRAG_NONE;
  win->wuss->furniture.repeat_step = 0;
  wuss__furniture_set_pressed(win, region);
}

void wuss__furniture_arm_drag(wuss_window_t             *win,
                              wuss_furniture_drag_kind_t kind,
                              point_t                    p,
                              wuss_button_t              button)
{
  wuss_t *wuss;
  point_t scroll;

  wuss = win->wuss;
  wuss_window_get_scroll(win, &scroll);

  wuss->furniture.dragging          = win;
  wuss->furniture.drag_kind         = kind;
  wuss->furniture.drag              = p;
  wuss->furniture.drag_scroll_start = scroll;
  wuss->furniture.drag_both         = (button & wuss_BUTTON_ADJUST) != 0;
}

wuss_window_t *wuss__furniture_release(wuss_t             *wuss,
                                       wuss_mouse_action_t action)
{
  wuss_window_t *win;

  win = wuss->furniture.dragging;
  if (action != wuss_MOUSE_UP || win == NULL)
    return NULL;

  wuss__furniture_set_pressed(win, wuss_FURNITURE_NONE);

  wuss->furniture.dragging  = NULL;
  wuss->furniture.drag_kind = wuss_FURNITURE_DRAG_NONE;

  return win;
}

int wuss__furniture_click(wuss_window_t      *win,
                          point_t             p,
                          wuss_button_t       button,
                          wuss_mouse_action_t action,
                          result_t           *rc)
{
  const wuss__furniture_element_t *element;

  element = wuss__furniture_element(win->wuss->furniture_ops->hit_test(win, p));
  if (element == NULL || element->press == NULL)
    return 0;

  *rc = element->press(win, element, p, button, action);

  return 1;
}
