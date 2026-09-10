/* wuss/window/set-hidden.c -- wuss - minimal window manager */

#include <stddef.h>

#include "wuss/task.h"

#include "../impl.h"

result_t wuss_window_set_hidden(wuss_window_t *window, int hidden)
{
  result_t     rc;
  wuss_event_t event;
  int          was_hidden;

  was_hidden = (window->flags & wuss_WINDOW_HIDDEN) != 0;
  if (!was_hidden == !hidden)
    return result_OK; /* no change */

  if (hidden)
  {
    wuss_t *wuss = window->wuss;

    /* Drop any pointer state that targets this window: an off-screen window
     * must not keep being driven by the mouse. Same set wuss_window_close
     * clears. */
#ifdef WUSS_FURNITURE
    if (wuss->furniture.dragging == window)
    {
      wuss->furniture.dragging  = NULL;
      wuss->furniture.drag_kind = wuss_FURNITURE_DRAG_NONE;
    }
#endif
#ifdef WUSS_ICONS
    if (wuss->pressed_window == window)
    {
      wuss->pressed_icon   = NULL;
      wuss->pressed_window = NULL;
    }
    if (wuss->hover_window == window)
    {
      wuss->hover_icon   = NULL;
      wuss->hover_window = NULL;
    }
#endif

    /* still on screen: repaint its footprint now, then mark it gone */
    wuss_invalidate(wuss, &window->visible);
    window->flags |= wuss_WINDOW_HIDDEN;
    return result_OK;
  }

  /* hidden -> visible: PRE_SHOW may veto, leaving the window hidden. */
  event.kind = wuss_EVENT_PRE_SHOW;
  rc = wuss__deliver(window->task, window, &event);
  if (rc != result_OK)
    return rc;

  /* mark it back, repaint its footprint so it appears, then SHOW */
  window->flags &= (wuss_window_flags_t) ~wuss_WINDOW_HIDDEN;
  wuss_invalidate(window->wuss, &window->visible);

  event.kind = wuss_EVENT_SHOW;
  (void) wuss__deliver(window->task, window, &event);

  return result_OK;
}
