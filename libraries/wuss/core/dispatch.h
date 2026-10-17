/* wuss/core/dispatch.h -- wuss - event delivery and the message queue */

#ifndef WUSS_CORE_DISPATCH_H
#define WUSS_CORE_DISPATCH_H

#include "types.h"


/* The single dispatch chokepoint. Delivers "ev" to "task"'s handle, passing
 * "win_or_null" as the window (NULL for task-view events). No-op (returns
 * result_OK) when task is NULL or has no handle. A debug build also asserts
 * ev->kind is valid for the recipient view. Every emit site routes through
 * here. */
result_t wuss__deliver(wuss_task_t        *task,
                       wuss_window_t      *win_or_null,
                       const wuss_event_t *ev);

/* wuss__deliver for an event that carries no payload: builds an event of
 * the given kind and delivers it. */
static inline result_t wuss__deliver_kind(wuss_task_t      *task,
                                          wuss_window_t    *win_or_null,
                                          wuss_event_kind_t kind)
{
  wuss_event_t event;

  event.kind = kind;
  return wuss__deliver(task, win_or_null, &event);
}

/* Bump dispatch_depth on entry to a public entry point (wuss_mouse_click,
 * wuss_mouse_move, wuss_key, wuss_idle); wuss__message_leave drops it back
 * and, at depth 0, drains the queue (see wuss__message_drain). Call these
 * paired around each entry point's whole body. */
void wuss__message_enter(wuss_t *wuss);
void wuss__message_leave(wuss_t *wuss);

/* Deliver every message currently queued, FIFO, including ones sent while
 * draining; stops (deferring the remainder to the next entry point) after
 * WUSS_MESSAGE_DRAIN_CAP deliveries in one call, logging a warning (and, in
 * an NDEBUG-less build, asserting) if that cap is hit. Only ever called at
 * dispatch_depth 0 -- see wuss__message_leave. */
void wuss__message_drain(wuss_t *wuss);

/* Purge "task" from the message queue: anything sent by "task" is dropped
 * unsent. Called by wuss_task_destroy before it unlinks/frees the task. */
void wuss__message_purge_task(wuss_t *wuss, wuss_task_t *task);

/* Purge "window" from the message queue: messages addressed to it are
 * removed and recorded ones bounce to their senders as
 * wuss_EVENT_MESSAGE_BOUNCED (delivered inline, before this returns). Called
 * by wuss_window_close while the window is still alive, so no queued message
 * ever outlives its target. */
void wuss__message_purge_window(wuss_t *wuss, wuss_window_t *window);
/* As wuss__message_purge_window, for messages addressed to a work-area or
 * icon bar icon about to be destroyed. */
void wuss__message_purge_icon(wuss_t *wuss, const void *icon);

/* Notify a window's task that it has been moved or resized, via
 * wuss_EVENT_OPEN; the return value is discarded, matching how furniture
 * drawing and other in-line notifications are treated. */
static inline void wuss__notify_open(wuss_window_t *window)
{
  (void) wuss__deliver_kind(window->task, window, wuss_EVENT_OPEN);
}

#endif /* WUSS_CORE_DISPATCH_H */
