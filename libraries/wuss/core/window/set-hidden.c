/* wuss/window/set-hidden.c -- wuss - minimal window manager */

#include <stddef.h>

#include "wuss/task.h"
#include "wuss/window.h"

#include "../impl.h"

result_t wuss_window_reveal_now(wuss_window_t *window)
{
  window->wuss->pre_show_proceed = 1;

  return result_OK;
}

result_t wuss_window_set_hidden(wuss_window_t *window, int hidden)
{
#ifdef WUSS_MENUS
  return wuss__window_set_hidden_ex(window, hidden, NULL, -1);
#else
  return wuss__window_set_hidden_ex(window, hidden);
#endif
}

#ifdef WUSS_MENUS
result_t wuss__window_set_hidden_ex(wuss_window_t     *window,
                                    int                hidden,
                                    struct wuss__menu *handle,
                                    int                index)
#else
result_t wuss__window_set_hidden_ex(wuss_window_t *window, int hidden)
#endif
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
    /* A hidden window is off the mouse; drop it from enter/exit tracking. No
     * EXIT event -- it stops receiving pointer events regardless. */
    wuss__pointer_forget_window(wuss, window);

    /* still on screen: repaint its footprint now, then mark it gone */
    wuss_invalidate(wuss, &window->visible);
    window->flags |= wuss_WINDOW_HIDDEN;
    return result_OK;
  }

  /* hidden -> visible: a plain window (handle == NULL, not a flagged menu
   * leaf -- an unflagged menu leaf never reaches this path at all, see
   * wuss__menu_open_window) proceeds by default; the handler only needs to
   * call wuss_window_reveal_now to react before it shows. A flagged menu
   * leaf is the opposite: the handler must call wuss_menu_open_window_now
   * to opt in, or the window stays hidden. A non-OK return here is a
   * genuine handler failure, not a veto. */
#ifdef WUSS_MENUS
  window->wuss->pre_show_proceed = (handle == NULL);
  event.data.pre_show.handle     = handle;
  event.data.pre_show.index      = index;
#else
  window->wuss->pre_show_proceed = 1;
#endif
  event.kind = wuss_EVENT_PRE_SHOW;
  rc = wuss__deliver(window->task, window, &event);
  if (rc != result_OK)
    return rc;
  if (!window->wuss->pre_show_proceed)
    return result_OK; /* handler did not opt in: stays hidden */

  /* mark it back, repaint its footprint so it appears, then SHOW */
  window->flags &= (wuss_window_flags_t) ~wuss_WINDOW_HIDDEN;
  wuss_invalidate(window->wuss, &window->visible);

  event.kind = wuss_EVENT_SHOW;
  (void) wuss__deliver(window->task, window, &event);

  return result_OK;
}
