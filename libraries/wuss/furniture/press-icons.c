/* wuss/furniture/press-icons.c -- wuss - clicks on close, back and toggle icons */

#include "../core/types.h"
#include "../core/dispatch.h"
#include "press.h"

result_t wuss__furniture_press_close(wuss_window_t                   *win,
                                     const wuss__furniture_element_t *element,
                                     point_t                          p,
                                     wuss_button_t                    button,
                                     wuss_mouse_action_t              action)
{
  wuss_event_t event;

  if (action == wuss_MOUSE_DOWN && (button & wuss_BUTTON_SELECT))
  {
    /* Lit even though the window may not survive the call below: a veto
     * leaves it armed for the ordinary MOUSE_UP release, and a successful
     * close tears the window down and wuss_window_close clears this pressed
     * state. */
    wuss__furniture_hold(win, element->region);

    /* User close-icon path: routes through try_close, so the task gets
     * PRE_CLOSE (may veto) then CLOSE and, if not vetoed, wuss tears the
     * window down. A veto's non-OK return propagates to the caller. */
    return wuss_window_try_close(win);
  }

  if (action == wuss_MOUSE_DOWN && (button & wuss_BUTTON_ADJUST))
  {
    /* Lit until the ordinary MOUSE_UP release, as for SELECT; the task may
     * close the window from the handler, which clears it. */
    wuss__furniture_hold(win, element->region);

    event.kind                        = wuss_EVENT_CLOSE_ADJUST;
    event.data.close_adjust.modifiers = win->wuss->modifiers;
    return wuss__deliver(win->task, win, &event);
  }

  /* The close icon sits in the titlebar, so any other press drags it. */
  return wuss__furniture_press_title(win, element, p, button, action);
}

result_t wuss__furniture_press_back(wuss_window_t                   *win,
                                    const wuss__furniture_element_t *element,
                                    point_t                          p,
                                    wuss_button_t                    button,
                                    wuss_mouse_action_t              action)
{
  NOT_USED(p);

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_SELECT)
    wuss_window_restack(win, wuss_ZORDER_BACK);
  else if (button & wuss_BUTTON_ADJUST)
    wuss_window_restack(win, wuss_ZORDER_FRONT);

  if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
    wuss__furniture_hold(win, element->region);

  return result_OK;
}

result_t wuss__furniture_press_toggle(wuss_window_t                   *win,
                                      const wuss__furniture_element_t *element,
                                      point_t                          p,
                                      wuss_button_t                    button,
                                      wuss_mouse_action_t              action)
{
  NOT_USED(p);

  if (action == wuss_MOUSE_DOWN &&
      (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
  {
    win->wuss->furniture_ops->toggle_size(win);

    wuss__furniture_hold(win, element->region);
  }

  return result_OK;
}
