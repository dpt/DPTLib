/* wuss/set-focus.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

result_t wuss_set_focus(wuss_t *wuss, wuss_window_t *window)
{
  wuss_window_t *was;

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

#ifdef WUSS_ICONS
  /* the caret only ever lives in the focus window */
  if (wuss->caret.window != window)
    wuss__caret_clear(wuss);
#endif

  /* a toolbar's focus shows in its parent's titlebar */
  if (was != NULL)
  {
    wuss__chrome_repaint(wuss__toolbar_root(was));
    (void) wuss__deliver_kind(was->task, was, wuss_EVENT_LOSE_FOCUS);
  }
  if (window != NULL)
  {
    wuss__chrome_repaint(wuss__toolbar_root(window));
    (void) wuss__deliver_kind(window->task, window, wuss_EVENT_GAIN_FOCUS);
  }

  return result_OK;
}
