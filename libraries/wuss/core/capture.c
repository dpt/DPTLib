/* wuss/capture.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

int wuss_is_capturing(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->capture != NULL;
}

/* Deliver a mouse event to the capturing window, wherever the pointer is.
 * The point is in that window's virtual content space, so it may lie outside
 * the content box. A MOUSE_UP ends the capture, after delivery. */
result_t wuss__capture_deliver(wuss_t             *wuss,
                               point_t             p,
                               wuss_button_t       button,
                               wuss_mouse_action_t action)
{
  wuss_window_t *win;
  box_t          content;
  wuss_event_t   event;

  win = wuss->capture;

  if (action == wuss_MOUSE_UP)
    wuss->capture = NULL;

  wuss__content_box(win, &content);

  event.kind              = wuss_EVENT_MOUSE;
  event.data.mouse.action = action;
  event.data.mouse.point  = POINT(p.x - content.x0 + win->scroll.x,
                                  p.y - content.y0 + win->scroll.y);
  event.data.mouse.button = button;
  return wuss__deliver(win->task, win, &event);
}

/* Called from wuss_window_close: the struct is about to be freed. */
void wuss__capture_forget_window(wuss_t *wuss, wuss_window_t *window)
{
  if (wuss->capture == window)
    wuss->capture = NULL;
}
