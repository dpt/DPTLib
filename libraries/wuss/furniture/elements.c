/* wuss/furniture/elements.c -- table of window furniture elements */

#include <stddef.h>

#include "base/utils.h"

#include "../core/impl.h"

/* In hit-test priority order. Grown hit boxes overlap where they meet, so
 * the order is load-bearing:
 *   - icons before TITLE: the titlebar corners belong to BACK / CLOSE / TOGGLE;
 *   - RESIZE before the scroll strips: the bottom-right corner is the resize
 *     icon when there is one;
 *   - VSCROLL before HSCROLL: with no resize icon the bottom-right corner
 *     resolves to VSCROLL_DOWN. */
const wuss__furniture_element_t wuss__furniture_elements[] =
{
  { wuss_FURNITURE_BACK,          wuss_WINDOW_BACK,        1,
    wuss__back_box,            wuss__back_hit_box,
    wuss__FURNITURE_PAINT_BACK,          wuss_FURNITURE_DRAG_NONE,            {  0,  0 } },
  { wuss_FURNITURE_CLOSE,         wuss_WINDOW_CLOSE,       1,
    wuss__close_box,           wuss__close_hit_box,
    wuss__FURNITURE_PAINT_CLOSE,         wuss_FURNITURE_DRAG_MOVE,            {  0,  0 } },
  { wuss_FURNITURE_TOGGLE_SIZE,   wuss_WINDOW_TOGGLE_SIZE, 1,
    wuss__toggle_box,          wuss__toggle_hit_box,
    wuss__FURNITURE_PAINT_TOGGLE,        wuss_FURNITURE_DRAG_NONE,            {  0,  0 } },
  { wuss_FURNITURE_TITLE,         0,                       1,
    NULL,                      wuss__title_hit_box,
    wuss__FURNITURE_PAINT_TITLE_BG,      wuss_FURNITURE_DRAG_MOVE,            {  0,  0 } },
  { wuss_FURNITURE_RESIZE,        wuss_WINDOW_RESIZE,      0,
    wuss__resize_box,          wuss__resize_hit_box,
    wuss__FURNITURE_PAINT_RESIZE,        wuss_FURNITURE_DRAG_RESIZE,          {  0,  0 } },
  { wuss_FURNITURE_VSCROLL_UP,    wuss_WINDOW_VSCROLL,     0,
    wuss__vscroll_up_box,      wuss__vscroll_up_hit_box,
    wuss__FURNITURE_PAINT_SCROLL_ARROWS, wuss_FURNITURE_DRAG_NONE,            {  0, -1 } },
  { wuss_FURNITURE_VSCROLL_DOWN,  wuss_WINDOW_VSCROLL,     0,
    wuss__vscroll_down_box,    wuss__vscroll_down_hit_box,
    wuss__FURNITURE_PAINT_SCROLL_ARROWS, wuss_FURNITURE_DRAG_NONE,            {  0,  1 } },
  { wuss_FURNITURE_VSCROLL_WELL,  wuss_WINDOW_VSCROLL,     0,
    wuss__vscroll_well_box,    wuss__vscroll_well_hit_box,
    wuss__FURNITURE_PAINT_SCROLL_WELLS,  wuss_FURNITURE_DRAG_VSCROLL_SAUSAGE, {  0,  0 } },
  { wuss_FURNITURE_HSCROLL_LEFT,  wuss_WINDOW_HSCROLL,     0,
    wuss__hscroll_left_box,    wuss__hscroll_left_hit_box,
    wuss__FURNITURE_PAINT_SCROLL_ARROWS, wuss_FURNITURE_DRAG_NONE,            { -1,  0 } },
  { wuss_FURNITURE_HSCROLL_RIGHT, wuss_WINDOW_HSCROLL,     0,
    wuss__hscroll_right_box,   wuss__hscroll_right_hit_box,
    wuss__FURNITURE_PAINT_SCROLL_ARROWS, wuss_FURNITURE_DRAG_NONE,            {  1,  0 } },
  { wuss_FURNITURE_HSCROLL_WELL,  wuss_WINDOW_HSCROLL,     0,
    wuss__hscroll_well_box,    wuss__hscroll_well_hit_box,
    wuss__FURNITURE_PAINT_SCROLL_WELLS,  wuss_FURNITURE_DRAG_HSCROLL_SAUSAGE, {  0,  0 } }
};

const int wuss__furniture_nelements = NELEMS(wuss__furniture_elements);

int wuss__furniture_element_present(const wuss_window_t             *window,
                                    const wuss__furniture_element_t *element)
{
  if (element->titlebar && (window->flags & wuss_WINDOW_NO_TITLEBAR))
    return 0;

  return element->flag == 0 || (window->flags & element->flag) != 0;
}

/* ponytail: linear scan of ~11 rows; index by region if it ever shows up in
 * a profile */
const wuss__furniture_element_t *wuss__furniture_element(wuss_furniture_region_t region)
{
  int i;

  for (i = 0; i < wuss__furniture_nelements; i++)
    if (wuss__furniture_elements[i].region == region)
      return &wuss__furniture_elements[i];

  return NULL;
}
