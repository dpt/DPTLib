/* wuss/set-focus.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

result_t wuss_set_focus(wuss_t *wuss, wuss_window_t *window)
{
  wuss_window_t *was;
  wuss_event_t   event;

  assert(wuss != NULL);

  if (window != NULL &&
      (!(window->flags & wuss_WINDOW_FOCUSABLE) ||
       (window->flags & wuss_WINDOW_HIDDEN)))
    return result_BAD_ARG;

  was = wuss->focus;
  if (window == was)
    return result_OK;

  /* Switch before delivering so both handlers, and any redraw they trigger,
   * already see the new owner. */
  wuss->focus = window;

  if (was != NULL)
  {
    wuss__chrome_repaint(was);
    event.kind = wuss_EVENT_LOSE_FOCUS;
    (void) wuss__deliver(was->task, was, &event);
  }
  if (window != NULL)
  {
    wuss__chrome_repaint(window);
    event.kind = wuss_EVENT_GAIN_FOCUS;
    (void) wuss__deliver(window->task, window, &event);
  }

  return result_OK;
}
