/* wuss/core/input.h -- wuss - pointer, focus, drag and capture state */

#ifndef WUSS_CORE_INPUT_H
#define WUSS_CORE_INPUT_H

#include "types.h"
#include "dispatch.h"


wuss_window_t *wuss__window_at(wuss_t *wuss, point_t p);

/* Core drag session (drag.c): wuss_drag_start's implementation plus the
 * per-move/end/tick/draw hooks called from mouse-move.c, mouse-click.c,
 * key.c, idle.c and redraw.c. See wuss_t::drag_window for the "is a drag
 * active" state. */
void wuss__drag_move(wuss_t *wuss, point_t p);
void wuss__drag_tick(wuss_t *wuss);
void wuss__drag_end(wuss_t        *wuss,
                    point_t        p,
                    wuss_window_t *drop,
                    int            cancelled);
void wuss__drag_forget_window(wuss_t *wuss, wuss_window_t *window);
void wuss__drag_draw(wuss_t *wuss);

/* Double-click detection (double-click.c): called for each MOUSE_DOWN with the
 * window it hit. Returns "button" with wuss_BUTTON_DOUBLE added when this press
 * completes a double-click, else unchanged; owns the wuss_t::last_down record. */
wuss_button_t wuss__double_click(wuss_t        *wuss,
                                 wuss_window_t *hit,
                                 point_t        p,
                                 wuss_button_t  button);

/* Mouse capture (capture.c): called from mouse-move.c and mouse-click.c while
 * wuss_t::capture is set, and from window/close.c. */
result_t wuss__capture_deliver(wuss_t             *wuss,
                               point_t             p,
                               wuss_button_t       button,
                               wuss_mouse_action_t action);
void wuss__capture_forget_window(wuss_t *wuss, wuss_window_t *window);

/* Move the "pointer is inside this window's footprint" tracker to "now"
 * (which may be NULL). If it changed, deliver wuss_EVENT_POINTER_EXIT to the
 * window it left and wuss_EVENT_POINTER_ENTER to the one it reached, in that
 * order. Called from every pointer-position update. Fires only on the
 * window-crossing edge; content<->furniture moves within one window keep the
 * same tracker and emit nothing. */
static inline void wuss__pointer_set_window(wuss_t *wuss, wuss_window_t *now)
{
  wuss_window_t *was;
  wuss_event_t   event;

  was = wuss->pointer_window;
  if (now == was)
    return;

  wuss->pointer_window = now;

  if (was != NULL)
  {
    event.kind = wuss_EVENT_POINTER_EXIT;
    (void) wuss__deliver(was->task, was, &event);
  }
  if (now != NULL)
  {
    event.kind = wuss_EVENT_POINTER_ENTER;
    (void) wuss__deliver(now->task, now, &event);
  }
}

/* Drop "window" from the pointer tracker without delivering EXIT -- for the
 * window teardown path, where wuss_EVENT_CLOSE already tells the task and the
 * window is about to be freed. No-op unless the pointer was inside it. */
static inline void wuss__pointer_forget_window(wuss_t        *wuss,
                                               wuss_window_t *window)
{
  if (wuss->pointer_window == window)
    wuss->pointer_window = NULL;
}

/* Drop "window" from the input focus without delivering LOSE_FOCUS -- for
 * the forced teardown path, which fires no events. No-op unless it held the
 * focus. */
static inline void wuss__focus_forget_window(wuss_t        *wuss,
                                             wuss_window_t *window)
{
  if (wuss->focus == window)
    wuss->focus = NULL;
}

#endif /* WUSS_CORE_INPUT_H */
