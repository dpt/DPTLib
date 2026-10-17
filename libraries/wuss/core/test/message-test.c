/* core/test/message-test.c -- wuss message queue tests */

#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/message.h"

#include "test/all-tests.h"

#ifdef WUSS_FURNITURE

#include "../impl.h"
#include "../../test/harness.h"
#ifdef WUSS_ICONBAR
#include "../../iconbar.h"
#endif

/* ----------------------------------------------------------------------- */

#define MSG_TEST_ACTION       (wuss_MESSAGE_APP_BASE + 1)
#define MSG_TEST_REPLY_ACTION (wuss_MESSAGE_APP_BASE + 2)

typedef struct msg_task
{
  wuss_t         *wuss;   /* set by the test before use; msg_handle
                           * has no other way to reach it */
  wuss_task_t    *self;
  int             message_count;
  int             bounced_count;
  wuss_message_t  last_message; /* a copy: the delivered message lives on
                                 * wuss's stack */
  int             ack_on_receipt; /* call wuss_acknowledge from the handler */
  int             reply_on_receipt; /* wuss_send back with your_ref set,
                                     * from the handler -- also counts as
                                     * an ack */
  wuss_window_t  *reply_target; /* window to address the reply to, or
                                 * NULL for the message's own sender */
}
msg_task_t;

static result_t msg_handle(wuss_window_t      *window,
                           const wuss_event_t *event,
                           void               *task_data)
{
  msg_task_t *mt;

  NOT_USED(window);

  mt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_MESSAGE:
    mt->message_count++;
    mt->last_message = *event->data.message;

    if (mt->reply_on_receipt)
      (void) wuss_send(mt->wuss, mt->self,
                       mt->reply_target ? mt->reply_target : NULL,
                       MSG_TEST_REPLY_ACTION, NULL, 0,
                       event->data.message->my_ref);
    else if (mt->ack_on_receipt)
      (void) wuss_acknowledge(mt->wuss, mt->self, event->data.message);
    break;

  case wuss_EVENT_MESSAGE_BOUNCED:
    mt->bounced_count++;
    mt->last_message = *event->data.message;
    break;

  default:
    break;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------- */

result_t wuss_message_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_send delivers a plain point-to-point message on the "
        "next entry-point drain, not inline\n");

  {
    static msg_task_t tc_recv;
    wuss_task_t      *delegate_send, *delegate_recv;
    wuss_window_t    *win_recv;
    box_t             box_recv;
    const char        payload[] = "hello";

    memset(&tc_recv, 0, sizeof(tc_recv));
    tc_recv.wuss = wuss;
    delegate_recv = mk_task(wuss, msg_handle, &tc_recv);
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    delegate_send = mk_task(wuss, NULL, NULL);
    if (delegate_send == NULL) goto Failure;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 50; box_recv.y1 = 50;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_send(wuss, delegate_send, win_recv, MSG_TEST_ACTION,
                   payload, sizeof(payload), 0);
    if (rc != result_OK)
      goto Failure;

    /* still queued: sending never delivers inline */
    if (tc_recv.message_count != 0)
      goto Failure;

    /* any public entry point drains the queue to empty before returning */
    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_recv.message_count != 1)
      goto Failure;
    if (tc_recv.last_message.action != MSG_TEST_ACTION)
      goto Failure;
    if (tc_recv.last_message.window != win_recv)
      goto Failure;
    if (tc_recv.last_message.sender != delegate_send)
      goto Failure;
    if (memcmp(tc_recv.last_message.data, payload, sizeof(payload)) != 0)
      goto Failure;

    wuss_window_close(win_recv);
    reap_test_tasks();
  }

  printf("test: wuss_send rejects a payload bigger than wuss_MESSAGE_DATA_SIZE\n");

  {
    static char toobig[wuss_MESSAGE_DATA_SIZE + 1];

    rc = wuss_send(wuss, mk_task(wuss, NULL, NULL), NULL, MSG_TEST_ACTION,
                   toobig, sizeof(toobig), 0);
    if (rc != result_WUSS_MESSAGE_TOO_BIG)
      goto Failure;

    reap_test_tasks();
  }

  printf("test: wuss_send_recorded acknowledged via wuss_acknowledge from "
        "the handler does not bounce\n");

  {
    static msg_task_t tc_recv;
    wuss_task_t      *delegate_send, *delegate_recv;
    wuss_window_t    *win_recv;
    box_t             box_recv;
    unsigned int      my_ref;

    memset(&tc_recv, 0, sizeof(tc_recv));
    tc_recv.wuss           = wuss;
    tc_recv.ack_on_receipt = 1;
    delegate_recv = mk_task(wuss, msg_handle, &tc_recv);
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    delegate_send = mk_task(wuss, NULL, NULL);
    if (delegate_send == NULL) goto Failure;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 50; box_recv.y1 = 50;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_send_recorded(wuss, delegate_send, win_recv, MSG_TEST_ACTION,
                            NULL, 0, 0, &my_ref);
    if (rc != result_OK || my_ref == 0)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_recv.message_count != 1)
      goto Failure;

    wuss_window_close(win_recv);
    reap_test_tasks();
  }

  printf("test: an unacknowledged recorded message bounces to the sender\n");

  {
    static msg_task_t tc_recv, tc_send;
    wuss_task_t      *delegate_send, *delegate_recv;
    wuss_window_t    *win_recv;
    box_t             box_recv;
    unsigned int      my_ref;

    memset(&tc_recv, 0, sizeof(tc_recv));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_recv.wuss = wuss;
    delegate_recv = mk_task(wuss, msg_handle, &tc_recv); /* no ack */
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    tc_send.wuss = wuss;
    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 50; box_recv.y1 = 50;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_send_recorded(wuss, delegate_send, win_recv, MSG_TEST_ACTION,
                            NULL, 0, 0, &my_ref);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_recv.message_count != 1)
      goto Failure;
    if (tc_send.bounced_count != 1)
      goto Failure;
    if (tc_send.last_message.my_ref != my_ref)
      goto Failure;

    wuss_window_close(win_recv);
    reap_test_tasks();
  }

#ifdef WUSS_ICONBAR
  printf("test: wuss_send_recorded_to an icon bar icon reaches the icon's owner and "
        "bounces if unacknowledged or the icon goes\n");

  {
    static msg_task_t        tc_recv, tc_send;
    wuss_iconbar_icon_spec_t spec;
    wuss_iconbar_icon_t     *icon;
    wuss_task_t             *delegate_send, *delegate_recv;
    unsigned int             my_ref;
    wuss_msg_target_t        target = { NULL, NULL };

    memset(&tc_recv, 0, sizeof(tc_recv));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_recv.wuss           = wuss;
    tc_recv.ack_on_receipt = 1;
    delegate_recv = mk_task(wuss, msg_handle, &tc_recv);
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    tc_send.wuss = wuss;
    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    memset(&spec, 0, sizeof(spec));
    spec.text = "M";
    rc = wuss_iconbar_icon_create(wuss, delegate_recv, &spec, &icon);
    if (rc != result_OK) goto Failure;
    target.window = wuss->iconbar.window;
    target.icon   = icon;

    /* acknowledged: delivered with no window, no bounce */
    rc = wuss_send_recorded_to(wuss, delegate_send, &target, MSG_TEST_ACTION,
                               NULL, 0, 0, &my_ref);
    if (rc != result_OK || my_ref == 0) goto Failure;
    rc = wuss_idle(wuss);
    if (rc != result_OK) goto Failure;
    if (tc_recv.message_count != 1 ||
        tc_recv.last_message.window != wuss->iconbar.window ||
        tc_recv.last_message.icon != icon || tc_send.bounced_count != 0)
      goto Failure;

    /* not acknowledged: bounces */
    tc_recv.ack_on_receipt = 0;
    rc = wuss_send_recorded_to(wuss, delegate_send, &target, MSG_TEST_ACTION,
                               NULL, 0, 0, &my_ref);
    if (rc != result_OK) goto Failure;
    rc = wuss_idle(wuss);
    if (rc != result_OK) goto Failure;
    if (tc_recv.message_count != 2 || tc_send.bounced_count != 1 ||
        tc_send.last_message.my_ref != my_ref)
      goto Failure;

    /* icon destroyed before the queue drains: bounces, never delivered */
    rc = wuss_send_recorded_to(wuss, delegate_send, &target, MSG_TEST_ACTION,
                               NULL, 0, 0, NULL);
    if (rc != result_OK) goto Failure;
    wuss_iconbar_icon_destroy(wuss, icon);
    rc = wuss_idle(wuss);
    if (rc != result_OK) goto Failure;
    if (tc_recv.message_count != 2 || tc_send.bounced_count != 2)
      goto Failure;

    reap_test_tasks();
  }
#endif

  printf("test: wuss_send_recorded_to a window's icon reaches the window's "
        "task with the icon, and bounces if the icon is deleted\n");

  {
    static msg_task_t tc_recv, tc_send;
    wuss_task_t      *delegate_send, *delegate_recv;
    wuss_window_t    *win_recv;
    wuss_icon_t      *icon;
    wuss_icon_spec_t  ispec;
    wuss_msg_target_t target = { NULL, NULL };
    box_t             box_recv;

    memset(&tc_recv, 0, sizeof(tc_recv));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_recv.wuss           = wuss;
    tc_recv.ack_on_receipt = 1;
    delegate_recv = mk_task(wuss, msg_handle, &tc_recv);
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    tc_send.wuss = wuss;
    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 90; box_recv.y1 = 90;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK) goto Failure;

    memset(&ispec, 0, sizeof(ispec));
    ispec.type = wuss_ICON_TYPE_LABEL;
    ispec.text = "I";
    ispec.bbox.x0 = 2;  ispec.bbox.y0 = 2;
    ispec.bbox.x1 = 20; ispec.bbox.y1 = 20;
    rc = wuss_icon_create(win_recv, &ispec, &icon);
    if (rc != result_OK) goto Failure;

    /* an icon without its window is no target */
    target.icon = icon;
    rc = wuss_send_recorded_to(wuss, delegate_send, &target, MSG_TEST_ACTION,
                               NULL, 0, 0, NULL);
    if (rc != result_BAD_ARG) goto Failure;

    target.window = win_recv;
    rc = wuss_send_recorded_to(wuss, delegate_send, &target, MSG_TEST_ACTION,
                               NULL, 0, 0, NULL);
    if (rc != result_OK) goto Failure;
    rc = wuss_idle(wuss);
    if (rc != result_OK) goto Failure;
    if (tc_recv.message_count != 1 || tc_recv.last_message.window != win_recv ||
        tc_recv.last_message.icon != icon || tc_send.bounced_count != 0)
      goto Failure;

    /* deleted before the queue drains: bounces, never delivered */
    rc = wuss_send_recorded_to(wuss, delegate_send, &target, MSG_TEST_ACTION,
                               NULL, 0, 0, NULL);
    if (rc != result_OK) goto Failure;
    wuss_icon_delete(win_recv, icon);
    rc = wuss_idle(wuss);
    if (rc != result_OK) goto Failure;
    if (tc_recv.message_count != 1 || tc_send.bounced_count != 1)
      goto Failure;

    wuss_window_close(win_recv);
    reap_test_tasks();
  }

  printf("test: replying with your_ref == my_ref from within the handler "
        "counts as acknowledging a recorded send\n");

  {
    static msg_task_t tc_recv, tc_send;
    wuss_task_t      *delegate_send, *delegate_recv;
    wuss_window_t    *win_recv, *win_send;
    box_t             box_recv, box_send;
    unsigned int      my_ref;

    memset(&tc_recv, 0, sizeof(tc_recv));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_recv.wuss             = wuss;
    tc_recv.reply_on_receipt = 1;
    delegate_recv = mk_task(wuss, msg_handle, &tc_recv);
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    tc_send.wuss = wuss;
    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 50; box_recv.y1 = 50;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK)
      goto Failure;

    box_send.x0 = 60; box_send.y0 = 10;
    box_send.x1 = 100; box_send.y1 = 50;
    rc = wuss_window_create(delegate_send, &box_send, "S",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_send);
    if (rc != result_OK)
      goto Failure;

    tc_recv.reply_target = win_send; /* address the reply back at the sender's window */

    rc = wuss_send_recorded(wuss, delegate_send, win_recv, MSG_TEST_ACTION,
                            NULL, 0, 0, &my_ref);
    if (rc != result_OK)
      goto Failure;

    /* the whole round trip -- send, deliver, reply-inside-handler (queued),
     * deliver the reply, decide not to bounce -- completes within this one
     * drain, per the priority rule (no public pump). */
    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_recv.message_count != 1)
      goto Failure;
    if (tc_send.bounced_count != 0)
      goto Failure;
    if (tc_send.message_count != 1) /* the reply, addressed to win_send */
      goto Failure;
    if (tc_send.last_message.action != MSG_TEST_REPLY_ACTION)
      goto Failure;
    if (tc_send.last_message.your_ref != my_ref)
      goto Failure;

    wuss_window_close(win_recv);
    wuss_window_close(win_send);
    reap_test_tasks();
  }

  printf("test: a broadcast (window == NULL) reaches every other task, not "
        "the sender, in registration order\n");

  {
    static msg_task_t tc_a, tc_b, tc_send;
    wuss_task_t      *delegate_a, *delegate_b, *delegate_send;

    memset(&tc_a, 0, sizeof(tc_a));
    memset(&tc_b, 0, sizeof(tc_b));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_a.wuss = wuss;
    tc_b.wuss = wuss;

    delegate_a = mk_task(wuss, msg_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;
    tc_a.self = delegate_a;

    delegate_b = mk_task(wuss, msg_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;
    tc_b.self = delegate_b;

    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    rc = wuss_send(wuss, delegate_send, NULL, MSG_TEST_ACTION, NULL, 0, 0);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_a.message_count != 1 || tc_b.message_count != 1)
      goto Failure;
    if (tc_send.message_count != 0) /* the sender never gets its own broadcast */
      goto Failure;

    reap_test_tasks();
  }

  printf("test: a recorded broadcast stops at the first acknowledgement\n");

  {
    static msg_task_t tc_a, tc_b, tc_send;
    wuss_task_t      *delegate_a, *delegate_b, *delegate_send;

    memset(&tc_a, 0, sizeof(tc_a));
    memset(&tc_b, 0, sizeof(tc_b));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_a.wuss           = wuss;
    tc_a.ack_on_receipt = 1; /* first registered task acks -- b must not be reached */
    tc_b.wuss           = wuss;
    tc_send.wuss        = wuss;

    delegate_a = mk_task(wuss, msg_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;
    tc_a.self = delegate_a;

    delegate_b = mk_task(wuss, msg_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;
    tc_b.self = delegate_b;

    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    rc = wuss_send_recorded(wuss, delegate_send, NULL, MSG_TEST_ACTION,
                            NULL, 0, 0, NULL);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_a.message_count != 1)
      goto Failure;
    if (tc_b.message_count != 0) /* a's ack stopped delivery before b */
      goto Failure;
    if (tc_send.bounced_count != 0)
      goto Failure;

    reap_test_tasks();
  }

  printf("test: a message addressed to a window that has since closed "
        "bounces as a dead endpoint\n");

  {
    static msg_task_t tc_send;
    wuss_task_t      *delegate_send, *delegate_recv;
    wuss_window_t    *win_recv;
    box_t             box_recv;

    memset(&tc_send, 0, sizeof(tc_send));
    tc_send.wuss = wuss;

    delegate_recv = mk_task(wuss, NULL, NULL);
    if (delegate_recv == NULL) goto Failure;

    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 50; box_recv.y1 = 50;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_send_recorded(wuss, delegate_send, win_recv, MSG_TEST_ACTION,
                            NULL, 0, 0, NULL);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_recv); /* still queued -- close it before it drains */

    /* bounced by the close itself: nothing may stay queued against the
     * freed window, for a later purge or reused address to trip over */
    if (tc_send.bounced_count != 1)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;

    if (tc_send.bounced_count != 1)
      goto Failure;

    reap_test_tasks();
  }

  printf("test: wuss_task_destroy purges the queue -- a recorded message to "
        "the doomed task bounces, one from it is dropped\n");

  {
    static msg_task_t tc_recv, tc_send;
    wuss_task_t      *delegate_send, *delegate_recv, *delegate_bystander;
    wuss_window_t    *win_recv;
    box_t             box_recv;

    memset(&tc_recv, 0, sizeof(tc_recv));
    memset(&tc_send, 0, sizeof(tc_send));
    tc_recv.wuss = wuss;
    tc_send.wuss = wuss;

    delegate_recv = mk_task(wuss, msg_handle, &tc_recv);
    if (delegate_recv == NULL) goto Failure;
    tc_recv.self = delegate_recv;

    delegate_send = mk_task(wuss, msg_handle, &tc_send);
    if (delegate_send == NULL) goto Failure;
    tc_send.self = delegate_send;

    /* a bystander task the doomed sender also messages -- must never see it,
     * since a message from a destroyed sender is dropped, not delivered */
    delegate_bystander = mk_task(wuss, NULL, NULL);
    if (delegate_bystander == NULL) goto Failure;

    box_recv.x0 = 10; box_recv.y0 = 10;
    box_recv.x1 = 50; box_recv.y1 = 50;
    rc = wuss_window_create(delegate_recv, &box_recv, "R",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_recv);
    if (rc != result_OK)
      goto Failure;

    /* queued, addressed to delegate_recv, which is about to be destroyed
     * still holding it -- must bounce back to delegate_send */
    rc = wuss_send_recorded(wuss, delegate_send, win_recv, MSG_TEST_ACTION,
                            NULL, 0, 0, NULL);
    if (rc != result_OK)
      goto Failure;

    /* queued, sent by delegate_recv (the one about to be destroyed) -- must
     * be dropped, not delivered to the bystander */
    rc = wuss_send(wuss, delegate_recv, NULL, MSG_TEST_ACTION, NULL, 0, 0);
    if (rc != result_OK)
      goto Failure;

    /* wuss_task_destroy purges the queue synchronously, before the queue
     * would otherwise drain -- no wuss_idle needed to see the bounce. */
    wuss_task_destroy(delegate_recv);
    mk_task_reg[0] = delegate_send; /* forget delegate_recv, already gone */
    mk_task_count  = 1;
    wuss_task_destroy(delegate_bystander);
    mk_task_count = 1;

    if (tc_send.bounced_count != 1)
      goto Failure;
    if (tc_recv.message_count != 0) /* destroyed before the queue reached it */
      goto Failure;

    reap_test_tasks();
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_message_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_message_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
