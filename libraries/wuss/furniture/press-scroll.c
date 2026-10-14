/* wuss/furniture/press-scroll.c -- wuss - clicks on scroll arrows and wells */

#include "../core/types.h"
#include "../core/geometry.h"
#include "press.h"

result_t wuss__furniture_press_arrow(wuss_window_t                   *win,
                                     const wuss__furniture_element_t *element,
                                     point_t                          p,
                                     wuss_button_t                    button,
                                     wuss_mouse_action_t              action)
{
  wuss_t *wuss;
  int     step;

  NOT_USED(p);

  if (action != wuss_MOUSE_DOWN ||
      !(button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
    return result_OK;

  wuss = win->wuss;

  /* Adjust-clicking an arrow steps the opposite way to the arrow it points,
   * so one arrow can be worked in both directions without moving the pointer.
   * Select wins a Select+Adjust chord, so a chord never scrolls backwards
   * unexpectedly. */
  step = (button & wuss_BUTTON_SELECT) ?  WUSS_SCROLL_STEP
                                       : -WUSS_SCROLL_STEP;

  wuss__scroll_step(win, POINT(element->step.x * step, element->step.y * step));

  /* Lit while held (MOUSE_UP clears it), and auto-repeating from wuss_idle
   * (see wuss__scroll_repeat). */
  wuss__furniture_hold(win, element->region);
  wuss->furniture.repeat_step   = step;
  wuss->furniture.repeat_frames = WUSS_SCROLL_REPEAT_DELAY;

  return result_OK;
}

result_t wuss__furniture_press_well(wuss_window_t                   *win,
                                    const wuss__furniture_element_t *element,
                                    point_t                          p,
                                    wuss_button_t                    button,
                                    wuss_mouse_action_t              action)
{
  box_t sausage;
  box_t content;
  int   vertical;
  int   sign, page, pos, lo, hi;

  if (action != wuss_MOUSE_DOWN ||
      !(button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
    return result_OK;

  /* A click landing in the well itself (not on the sausage) pages the content
   * one visible extent towards the click, RISC OS style, keeping one step of
   * overlap. ADJUST reverses that -- it pages away from the click instead,
   * matching RISC OS's SELECT/ADJUST mirroring elsewhere (e.g. the arrow
   * icons). A direct hit on the sausage itself pages nothing and falls
   * through to arm the drag below. */
  vertical = element->region == wuss_FURNITURE_VSCROLL_WELL;
  sign     = (button & wuss_BUTTON_ADJUST) ? -1 : 1;

  wuss__content_box(win, &content);

  if (vertical)
  {
    wuss__vscroll_sausage_box(win, &sausage);
    page = (content.y1 - content.y0) - WUSS_SCROLL_STEP;
    pos  = p.y;
    lo   = sausage.y0;
    hi   = sausage.y1;
  }
  else
  {
    wuss__hscroll_sausage_box(win, &sausage);
    page = (content.x1 - content.x0) - WUSS_SCROLL_STEP;
    pos  = p.x;
    lo   = sausage.x0;
    hi   = sausage.x1;
  }
  page = MAX(page, 1);

  if (pos < lo || pos > hi)
  {
    page *= (pos < lo) ? -sign : sign;
    wuss__scroll_step(win, vertical ? POINT(0, page) : POINT(page, 0));

    /* A well click that paged must not also arm a sausage drag, or holding
     * the button after the page would start scrolling live as though the
     * pointer had grabbed the sausage itself. */
    return result_OK;
  }

  wuss__furniture_arm_drag(win, element->drag_kind, p, button);

  return result_OK;
}
