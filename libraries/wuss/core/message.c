/* wuss/message.c -- inter-task messaging: queue, send/ack, drain, purge */

#include <assert.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "wuss/message.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* Next my_ref to hand out, skipping 0 on wrap: 0 is reserved to mean "fresh
 * send" in your_ref. */
static unsigned int next_ref(wuss_t *wuss)
{
  unsigned int r;

  r = wuss->msg.next_ref++;
  if (wuss->msg.next_ref == 0)
    wuss->msg.next_ref = 1;

  return r;
}

/* Append "msg" to the tail of the queue. Returns result_OK or result_OOM. */
static result_t enqueue(wuss_t *wuss, const wuss_message_t *msg)
{
  if (wuss__array_grow(&wuss->alloc, (void **) &wuss->msg.queue,
                       sizeof(*wuss->msg.queue), wuss->msg.count,
                       &wuss->msg.cap, 1, 8) != 0)
    return result_OOM;

  wuss->msg.queue[wuss->msg.count++] = *msg;

  return result_OK;
}

static result_t send_common(wuss_t                  *wuss,
                            wuss_task_t             *sender,
                            const wuss_msg_target_t *target,
                            int                      action,
                            const void              *data,
                            size_t                   size,
                            unsigned int             your_ref,
                            int                      recorded,
                            unsigned int            *my_ref_out)
{
  wuss_message_t msg;
  unsigned int   my_ref;

  if (size > wuss_MESSAGE_DATA_SIZE)
    return result_WUSS_MESSAGE_TOO_BIG;

  /* an icon needs its window */
  if (target->icon != NULL && target->window == NULL)
    return result_BAD_ARG;

  my_ref = recorded ? next_ref(wuss) : 0;

  if (your_ref != 0 && wuss->msg.acking != NULL && your_ref == wuss->msg.acking->my_ref)
    wuss->msg.acked = 1;

  msg.size     = size;
  msg.action   = action;
  msg.sender   = sender;
  msg.window   = target->window;
  msg.icon     = target->icon;
  msg.my_ref   = my_ref;
  msg.your_ref = your_ref;
  msg.recorded = recorded;
  if (data != NULL)
    memcpy(msg.data, data, size);

  if (my_ref_out != NULL)
    *my_ref_out = my_ref;

  return enqueue(wuss, &msg);
}

result_t wuss_send_to(wuss_t                  *wuss,
                      wuss_task_t             *sender,
                      const wuss_msg_target_t *target,
                      int                      action,
                      const void              *data,
                      size_t                   size,
                      unsigned int             your_ref)
{
  return send_common(wuss, sender, target, action, data, size, your_ref, 0,
                     NULL);
}

result_t wuss_send_recorded_to(wuss_t                  *wuss,
                               wuss_task_t             *sender,
                               const wuss_msg_target_t *target,
                               int                      action,
                               const void              *data,
                               size_t                   size,
                               unsigned int             your_ref,
                               unsigned int            *my_ref)
{
  return send_common(wuss, sender, target, action, data, size, your_ref, 1,
                     my_ref);
}

result_t wuss_send(wuss_t        *wuss,
                   wuss_task_t   *sender,
                   wuss_window_t *window,
                   int            action,
                   const void    *data,
                   size_t         size,
                   unsigned int   your_ref)
{
  wuss_msg_target_t target = { NULL, NULL };

  target.window = window;
  return wuss_send_to(wuss, sender, &target, action, data, size, your_ref);
}

result_t wuss_send_recorded(wuss_t        *wuss,
                            wuss_task_t   *sender,
                            wuss_window_t *window,
                            int            action,
                            const void    *data,
                            size_t         size,
                            unsigned int   your_ref,
                            unsigned int  *my_ref)
{
  wuss_msg_target_t target = { NULL, NULL };

  target.window = window;
  return wuss_send_recorded_to(wuss, sender, &target, action, data, size,
                               your_ref, my_ref);
}

result_t wuss_acknowledge(wuss_t               *wuss,
                          wuss_task_t          *recipient,
                          const wuss_message_t *msg)
{
  wuss_message_t ack;
  wuss_event_t   event;

  if (wuss->msg.acking != NULL && msg->my_ref == wuss->msg.acking->my_ref)
    wuss->msg.acked = 1;

  /* A recorded DataLoad completes the transfer: tell the saver, in its task
   * view (a task has no single window; the saver matches on your_ref). */
  if (msg->action == wuss_MESSAGE_DATA_LOAD && msg->recorded)
  {
    memset(&ack, 0, sizeof(ack));
    ack.action   = wuss_MESSAGE_DATA_LOAD_ACK;
    ack.sender   = recipient;
    ack.your_ref = msg->my_ref;

    event.kind         = wuss_EVENT_MESSAGE;
    event.data.message = &ack;
    (void) wuss__deliver(msg->sender, NULL, &event);
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* Is "window" still a live window of "wuss"? Point-to-point delivery and the
 * dead-endpoint bounce both need this -- a window can close between a send
 * and the queue draining it. */
static int window_alive(wuss_t *wuss, wuss_window_t *window)
{
  wuss_window_t *w;

  for (w = wuss__z_first(wuss); w != NULL; w = wuss__z_below(w))
    if (w == window)
      return 1;

  return 0;
}

static void bounce(wuss_t *wuss, const wuss_message_t *msg)
{
  wuss_event_t event;

  NOT_USED(wuss);

  if (!msg->recorded)
    return;

  event.kind         = wuss_EVENT_MESSAGE_BOUNCED;
  event.data.message = msg;
  (void) wuss__deliver(msg->sender, NULL, &event);
}

/* Deliver one message to one task (the point-to-point recipient, or one
 * broadcast recipient), tracking the ack state for the bounce-suppression
 * rule. Returns non-zero if the recipient acknowledged it. */
static int deliver_one(wuss_t               *wuss,
                       wuss_task_t          *task,
                       wuss_window_t        *window,
                       const wuss_message_t *msg)
{
  const wuss_message_t *saved_acking;
  int                   saved_acked;
  wuss_event_t          event;
  int                   acked;

  saved_acking = wuss->msg.acking;
  saved_acked  = wuss->msg.acked;

  wuss->msg.acking = msg;
  wuss->msg.acked  = 0;

  event.kind         = wuss_EVENT_MESSAGE;
  event.data.message = msg;
  (void) wuss__deliver(task, window, &event);

  acked = wuss->msg.acked;

  wuss->msg.acking = saved_acking;
  wuss->msg.acked  = saved_acked;

  return acked;
}

static void deliver(wuss_t *wuss, const wuss_message_t *msg)
{
#ifdef WUSS_ICONBAR
  /* An icon on the icon bar: its owner, not the bar's own task, gets it, and
   * in the task view. An icon destroyed since the send has had its messages
   * purged, so it is live here. */
  if (msg->icon != NULL && msg->window == wuss->iconbar.window)
  {
    const wuss_iconbar_icon_t *bar_icon = msg->icon;

    if (!deliver_one(wuss, bar_icon->owner, NULL, msg))
      bounce(wuss, msg);
    return;
  }
#endif

  if (msg->window != NULL)
  {
    if (!window_alive(wuss, msg->window))
    {
      bounce(wuss, msg);
      return;
    }

    if (deliver_one(wuss, msg->window->task, msg->window, msg) ||
        !msg->recorded)
      return;

    bounce(wuss, msg);
  }
  else
  {
    list_t *e;
    int     acked;

    acked = 0;
    for (e = wuss->tasks.next; e != NULL; )
    {
      wuss_task_t *task;

      task = wuss__task_from_link(e);
      e    = e->next; /* as wuss_idle: the handler may free its own node */
      if (task == msg->sender)
        continue;

      if (deliver_one(wuss, task, NULL, msg))
      {
        acked = 1;
        if (msg->recorded)
          break; /* first ack stops further delivery of a recorded broadcast */
      }
    }

    if (msg->recorded && !acked)
      bounce(wuss, msg);
  }
}

/* ----------------------------------------------------------------------- */

void wuss__message_enter(wuss_t *wuss)
{
  wuss->msg.dispatch_depth++;
}

void wuss__message_leave(wuss_t *wuss)
{
  wuss->msg.dispatch_depth--;
  if (wuss->msg.dispatch_depth == 0)
    wuss__message_drain(wuss);
}

void wuss__message_drain(wuss_t *wuss)
{
  int ndelivered;

  wuss->msg.dispatch_depth++; /* deliver() must not re-drain if it re-enters */

  ndelivered = 0;
  while (wuss->msg.count > 0)
  {
    wuss_message_t msg;

    if (ndelivered >= WUSS_MESSAGE_DRAIN_CAP)
    {
      logf_warning("wuss: %d messages delivered in one drain, deferring the "
                   "remainder to the next entry point", ndelivered);
      assert(ndelivered < WUSS_MESSAGE_DRAIN_CAP);
      break;
    }

    msg = wuss->msg.queue[0];
    memmove(&wuss->msg.queue[0], &wuss->msg.queue[1],
           (size_t) (wuss->msg.count - 1) * sizeof(*wuss->msg.queue));
    wuss->msg.count--;

    deliver(wuss, &msg);
    ndelivered++;
  }

  wuss->msg.dispatch_depth--;
}

void wuss__message_purge_task(wuss_t *wuss, wuss_task_t *task)
{
  int i;

  /* Drop unsent: nothing to bounce, the sender is gone. Messages addressed
   * to the task's windows are bounced as wuss_window_close reaps each one. */
  for (i = 0; i < wuss->msg.count; )
  {
    if (wuss->msg.queue[i].sender == task)
    {
      memmove(&wuss->msg.queue[i], &wuss->msg.queue[i + 1],
             (size_t) (wuss->msg.count - i - 1) * sizeof(*wuss->msg.queue));
      wuss->msg.count--;
      continue;
    }

    i++;
  }
}

void wuss__message_purge_window(wuss_t *wuss, wuss_window_t *window)
{
  int i;

  for (i = 0; i < wuss->msg.count; )
  {
    if (wuss->msg.queue[i].window == window)
    {
      wuss_message_t doomed;

      doomed = wuss->msg.queue[i];
      memmove(&wuss->msg.queue[i], &wuss->msg.queue[i + 1],
             (size_t) (wuss->msg.count - i - 1) * sizeof(*wuss->msg.queue));
      wuss->msg.count--;

      bounce(wuss, &doomed);
      continue;
    }

    i++;
  }
}

void wuss__message_purge_icon(wuss_t *wuss, const void *icon)
{
  int i;

  for (i = 0; i < wuss->msg.count; )
  {
    if (wuss->msg.queue[i].icon == icon)
    {
      wuss_message_t doomed;

      doomed = wuss->msg.queue[i];
      memmove(&wuss->msg.queue[i], &wuss->msg.queue[i + 1],
             (size_t) (wuss->msg.count - i - 1) * sizeof(*wuss->msg.queue));
      wuss->msg.count--;

      bounce(wuss, &doomed);
      continue;
    }

    i++;
  }
}
