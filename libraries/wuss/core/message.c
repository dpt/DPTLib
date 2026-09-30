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

  r = wuss->next_ref++;
  if (wuss->next_ref == 0)
    wuss->next_ref = 1;

  return r;
}

/* Append "msg" to the tail of the queue. Returns result_OK or result_OOM. */
static result_t enqueue(wuss_t *wuss, const wuss_message_t *msg)
{
  if (wuss__array_grow(&wuss->alloc, (void **) &wuss->queue,
                       sizeof(*wuss->queue), wuss->nqueued,
                       &wuss->cap_queue, 1, 8) != 0)
    return result_OOM;

  wuss->queue[wuss->nqueued++] = *msg;

  return result_OK;
}

static result_t send_common(wuss_t        *wuss,
                            wuss_task_t   *sender,
                            wuss_window_t *window,
                            int            action,
                            const void    *data,
                            size_t         size,
                            unsigned int   your_ref,
                            int            recorded,
                            unsigned int  *my_ref_out)
{
  wuss_message_t msg;
  unsigned int   my_ref;

  if (size > wuss_MESSAGE_DATA_SIZE)
    return result_WUSS_MESSAGE_TOO_BIG;

  my_ref = recorded ? next_ref(wuss) : 0;

  if (your_ref != 0 && wuss->acking != NULL && your_ref == wuss->acking->my_ref)
    wuss->acked = 1;

  msg.size     = size;
  msg.action   = action;
  msg.sender   = sender;
  msg.window   = window;
  msg.my_ref   = my_ref;
  msg.your_ref = your_ref;
  msg.recorded = recorded;
  if (data != NULL)
    memcpy(msg.data, data, size);

  if (my_ref_out != NULL)
    *my_ref_out = my_ref;

  return enqueue(wuss, &msg);
}

result_t wuss_send(wuss_t        *wuss,
                   wuss_task_t   *sender,
                   wuss_window_t *window,
                   int            action,
                   const void    *data,
                   size_t         size,
                   unsigned int   your_ref)
{
  return send_common(wuss, sender, window, action, data, size, your_ref, 0,
                     NULL);
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
  return send_common(wuss, sender, window, action, data, size, your_ref, 1,
                     my_ref);
}

result_t wuss_acknowledge(wuss_t               *wuss,
                          wuss_task_t          *recipient,
                          const wuss_message_t *msg)
{
  NOT_USED(recipient);

  if (wuss->acking != NULL && msg->my_ref == wuss->acking->my_ref)
    wuss->acked = 1;

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

  saved_acking = wuss->acking;
  saved_acked  = wuss->acked;

  wuss->acking = msg;
  wuss->acked  = 0;

  event.kind         = wuss_EVENT_MESSAGE;
  event.data.message = msg;
  (void) wuss__deliver(task, window, &event);

  acked = wuss->acked;

  wuss->acking = saved_acking;
  wuss->acked  = saved_acked;

  return acked;
}

static void deliver(wuss_t *wuss, const wuss_message_t *msg)
{
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
    for (e = wuss->tasks.next; e != NULL; e = e->next)
    {
      wuss_task_t *task;

      task = wuss__task_from_link(e);
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
  wuss->dispatch_depth++;
}

void wuss__message_leave(wuss_t *wuss)
{
  wuss->dispatch_depth--;
  if (wuss->dispatch_depth == 0)
    wuss__message_drain(wuss);
}

void wuss__message_drain(wuss_t *wuss)
{
  int ndelivered;

  wuss->dispatch_depth++; /* deliver() must not re-drain if it re-enters */

  ndelivered = 0;
  while (wuss->nqueued > 0)
  {
    wuss_message_t msg;

    if (ndelivered >= WUSS_MESSAGE_DRAIN_CAP)
    {
      logf_warning("wuss: %d messages delivered in one drain, deferring the "
                   "remainder to the next entry point", ndelivered);
      assert(ndelivered < WUSS_MESSAGE_DRAIN_CAP);
      break;
    }

    msg = wuss->queue[0];
    memmove(&wuss->queue[0], &wuss->queue[1],
           (size_t) (wuss->nqueued - 1) * sizeof(*wuss->queue));
    wuss->nqueued--;

    deliver(wuss, &msg);
    ndelivered++;
  }

  wuss->dispatch_depth--;
}

void wuss__message_purge_task(wuss_t *wuss, wuss_task_t *task)
{
  int i;

  for (i = 0; i < wuss->nqueued; )
  {
    wuss_message_t *msg;

    msg = &wuss->queue[i];

    if (msg->sender == task)
    {
      /* Drop unsent: nothing to bounce, the sender is gone. */
      memmove(&wuss->queue[i], &wuss->queue[i + 1],
             (size_t) (wuss->nqueued - i - 1) * sizeof(*wuss->queue));
      wuss->nqueued--;
      continue;
    }

    if (msg->window != NULL && msg->window->task == task)
    {
      wuss_message_t doomed;

      doomed = *msg;
      memmove(&wuss->queue[i], &wuss->queue[i + 1],
             (size_t) (wuss->nqueued - i - 1) * sizeof(*wuss->queue));
      wuss->nqueued--;

      bounce(wuss, &doomed);
      continue;
    }

    i++;
  }
}
