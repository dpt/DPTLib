/* wuss/furniture/press-drag.c -- wuss - clicks that start a move or resize drag */

#include "../core/types.h"
#include "../core/geometry.h"
#include "press.h"

result_t wuss__furniture_press_title(wuss_window_t                   *win,
                                     const wuss__furniture_element_t *element,
                                     point_t                          p,
                                     wuss_button_t                    button,
                                     wuss_mouse_action_t              action)
{
  wuss_t *wuss;
  box_t   content;

  NOT_USED(element);

  if (action != wuss_MOUSE_DOWN || !(button & WUSS_BUTTON_CLICK))
    return result_OK;

  wuss = win->wuss;

  if (button & wuss_BUTTON_SELECT)
    wuss_window_restack(win, wuss_ZORDER_FRONT);

  wuss__content_box(win, &content);
  wuss->furniture.dragging  = win;
  wuss->furniture.drag_kind = wuss_FURNITURE_DRAG_MOVE;
  wuss->furniture.drag.x    = p.x - content.x0;
  wuss->furniture.drag.y    = p.y - content.y0;

  return result_OK;
}

result_t wuss__furniture_press_resize(wuss_window_t                   *win,
                                      const wuss__furniture_element_t *element,
                                      point_t                          p,
                                      wuss_button_t                    button,
                                      wuss_mouse_action_t              action)
{
  wuss_t *wuss;
  box_t   content;

  if (action != wuss_MOUSE_DOWN || !(button & WUSS_BUTTON_CLICK))
    return result_OK;

  wuss = win->wuss;

  if (button & wuss_BUTTON_SELECT)
    wuss_window_restack(win, wuss_ZORDER_FRONT);

  wuss__furniture_arm_drag(win, element->drag_kind, p, button);

  /* The pointer's offset from the content box's current bottom-right corner,
   * so the point grabbed on the resize icon stays under the pointer as it
   * moves, rather than that corner jumping to meet the pointer on the very
   * first move. */
  wuss__content_box(win, &content);
  wuss->furniture.drag_offset.x = p.x - content.x1;
  wuss->furniture.drag_offset.y = p.y - content.y1;

  wuss__furniture_set_pressed(win, element->region);

  return result_OK;
}
