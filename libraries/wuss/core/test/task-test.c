/* core/test/task-test.c -- wuss task lifecycle and broadcast tests */

#include <stdio.h>
#include <stdlib.h>
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
#include "wuss/icon-spec.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */




/* An IDLE handler that closes a window the first time it is broadcast to. On
 * an autoclose task with that as its only window, the close reaps the task
 * and frees its list node from inside the wuss_idle / wuss_set_palette walk;
 * the test checks the walk survives it and still reaches the tasks behind. */
static wuss_window_t *g_close_on_idle_win;

static result_t close_on_idle_handle(wuss_window_t      *window,
                                     const wuss_event_t *event,
                                     void               *task_data)
{
  test_task_t *tc;

  NOT_USED(window);

  tc = task_data;

  if (event->kind == wuss_EVENT_IDLE)
  {
    tc->idle_count++;
    if (g_close_on_idle_win != NULL)
    {
      wuss_window_t *doomed;

      doomed              = g_close_on_idle_win;
      g_close_on_idle_win = NULL;
      wuss_window_close(doomed); /* reaps this task mid-walk */
    }
    return result_OK;
  }

  if (event->kind == wuss_EVENT_QUIT)
    tc->stop_count++;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t wuss_task_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_set_palette and wuss_idle broadcast once per registered task\n");
  {
    /* the wuss under test was made with a 2-entry palette (see top of this
     * function); a fresh 2-entry palette must broadcast to every task, a
     * wrong-length one must be refused without broadcasting */
    static const colour_t swapped[2] = { { 0xFF202020 }, { 0xFFE0E0E0 } };
    static const colour_t too_long[3] =
      { { 0xFF000000 }, { 0xFF808080 }, { 0xFFFFFFFF } };

    static test_task_t tc_pa, tc_pb;
    wuss_task_t       *delegate_pa, *delegate_pb;

    /* every earlier block-local test_task_t is out of scope now; drop the
     * tasks that still point at it so the broadcast counts are exactly the
     * two made here */
    reap_test_tasks();

    memset(&tc_pa, 0, sizeof(tc_pa));
    memset(&tc_pb, 0, sizeof(tc_pb));
    delegate_pa = mk_task(wuss, test_handle, &tc_pa);
    delegate_pb = mk_task(wuss, test_handle, &tc_pb);
    if (delegate_pa == NULL || delegate_pb == NULL)
      goto Failure;

    rc = wuss_set_palette(wuss, swapped, NELEMS(swapped));
    if (rc != result_OK)
      goto Failure;
    if (tc_pa.palette_count != 1 || tc_pb.palette_count != 1)
      goto Failure; /* one PALETTE per registered task */

    rc = wuss_set_palette(wuss, too_long, NELEMS(too_long));
    if (rc != result_BAD_ARG)
      goto Failure;
    if (tc_pa.palette_count != 1 || tc_pb.palette_count != 1)
      goto Failure; /* rejected call must not have broadcast */

    /* wuss_idle broadcasts a single wuss_EVENT_IDLE to each task too; a
     * task with no window still gets it (delivery is per registered task,
     * not per window) */
    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_pa.idle_count != 1 || tc_pb.idle_count != 1)
      goto Failure;

    reap_test_tasks(); /* tc_pa/tc_pb die with this block too */
    rc = result_OK;
  }

  printf("test: wuss_task_destroy sends one wuss_EVENT_QUIT and closes the task's windows\n");
  {
    static test_task_t tc_q;
    wuss_task_t       *delegate_q;
    box_t              box_q;
    wuss_window_t     *win_q1, *win_q2;

    memset(&tc_q, 0, sizeof(tc_q));
    delegate_q = mk_task(wuss, test_handle, &tc_q);
    if (delegate_q == NULL) goto Failure;

    box_q.x0 = 0;  box_q.y0 = 0;
    box_q.x1 = 40; box_q.y1 = 40;
    rc = wuss_window_create(delegate_q, &box_q, "Q1", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_q), SIZE2D(0, 0), &win_q1);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_window_create(delegate_q, &box_q, "Q2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_q), SIZE2D(0, 0), &win_q2);
    if (rc != result_OK)
      goto Failure;
    NOT_USED(win_q1);
    NOT_USED(win_q2);

    /* one QUIT for the task, regardless of how many windows it owns */
    wuss_task_destroy(delegate_q);
    if (tc_q.stop_count != 1)
      goto Failure;

    mk_task_count = 0; /* delegate_q is gone; drop the stale registry entry */
  }

  printf("test: an autoclose task self-destructs when its last window closes and stops receiving broadcasts\n");
  {
    static test_task_t tc_ac;
    wuss_task_t       *delegate_ac;
    box_t              box_ac;
    wuss_window_t     *win_ac1, *win_ac2;

    memset(&tc_ac, 0, sizeof(tc_ac));
    delegate_ac = mk_task(wuss, test_handle, &tc_ac);
    if (delegate_ac == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_ac, 1);

    box_ac.x0 = 0;  box_ac.y0 = 0;
    box_ac.x1 = 40; box_ac.y1 = 40;
    rc = wuss_window_create(delegate_ac, &box_ac, "AC1", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_ac), SIZE2D(0, 0), &win_ac1);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_window_create(delegate_ac, &box_ac, "AC2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_ac), SIZE2D(0, 0), &win_ac2);
    if (rc != result_OK)
      goto Failure;

    /* first close: task still owns a window, so no reap and no QUIT */
    wuss_window_close(win_ac1);
    if (tc_ac.stop_count != 0)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_ac.idle_count != 1) /* still registered, still broadcast to */
      goto Failure;

    /* last close: reap fires one QUIT and unregisters the task */
    wuss_window_close(win_ac2);
    if (tc_ac.stop_count != 1)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_ac.idle_count != 1) /* gone: no further broadcast */
      goto Failure;

    mk_task_count = 0; /* delegate_ac reaped itself; drop the stale entry */
  }

  printf("test: destroying a parent task destroys the tasks created under it\n");
  {
    static test_task_t tc_par, tc_kid1, tc_kid2, tc_other;
    wuss_task_t       *par, *kid1, *kid2, *other;

    memset(&tc_par, 0, sizeof(tc_par));
    memset(&tc_kid1, 0, sizeof(tc_kid1));
    memset(&tc_kid2, 0, sizeof(tc_kid2));
    memset(&tc_other, 0, sizeof(tc_other));

    par = mk_task(wuss, test_handle, &tc_par);
    if (par == NULL) goto Failure;

    wuss_set_task_parent(wuss, par);
    kid1 = mk_task(wuss, test_handle, &tc_kid1);
    kid2 = mk_task(wuss, test_handle, &tc_kid2);
    wuss_set_task_parent(wuss, NULL);
    other = mk_task(wuss, test_handle, &tc_other);
    if (kid1 == NULL || kid2 == NULL || other == NULL) goto Failure;

    wuss_task_destroy(par);
    mk_task_count = 0; /* par and its kids are gone; drop the stale entries */
    mk_task_reg[mk_task_count++] = other;

    if (tc_par.stop_count != 1 || tc_kid1.stop_count != 1 ||
        tc_kid2.stop_count != 1 || tc_other.stop_count != 0)
      goto Failure;
  }

  printf("test: a task reaping itself from inside wuss_idle does not derail the task walk\n");
  {
    static test_task_t tc_w1, tc_w2, tc_w3;
    wuss_task_t       *delegate_w1, *delegate_w2, *delegate_w3;
    box_t              box_w;
    wuss_window_t     *win_w1, *win_w3;

    memset(&tc_w1, 0, sizeof(tc_w1));
    memset(&tc_w2, 0, sizeof(tc_w2));
    memset(&tc_w3, 0, sizeof(tc_w3));

    box_w.x0 = 0;  box_w.y0 = 0;
    box_w.x1 = 40; box_w.y1 = 40;

    /* w1 first, w2 (self-reaping, autoclose) in the middle, w3 last: the
     * walk must deliver to w1, survive w2 freeing its own node, and still
     * reach w3. */
    delegate_w1 = mk_task(wuss, test_handle, &tc_w1);
    if (delegate_w1 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w1, 1);
    rc = wuss_window_create(delegate_w1, &box_w, "W1", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_w), SIZE2D(0, 0), &win_w1);
    if (rc != result_OK)
      goto Failure;

    delegate_w2 = mk_task(wuss, close_on_idle_handle, &tc_w2);
    if (delegate_w2 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w2, 1);
    rc = wuss_window_create(delegate_w2, &box_w, "W2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_w), SIZE2D(0, 0),
                            &g_close_on_idle_win);
    if (rc != result_OK)
      goto Failure;

    delegate_w3 = mk_task(wuss, test_handle, &tc_w3);
    if (delegate_w3 == NULL) goto Failure;
    wuss_task_set_autoclose(delegate_w3, 1);
    rc = wuss_window_create(delegate_w3, &box_w, "W3", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_w), SIZE2D(0, 0), &win_w3);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_w1.idle_count != 1) /* reached before the reap */
      goto Failure;
    if (tc_w2.stop_count != 1) /* self-reaped mid-walk */
      goto Failure;
    if (tc_w3.idle_count != 1) /* walk still reached the task behind w2 */
      goto Failure;

    /* w2 is gone; a second idle must skip it cleanly */
    rc = wuss_idle(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_w2.idle_count != 1 || tc_w3.idle_count != 2)
      goto Failure;

    wuss_window_close(win_w1); /* autoclose: reaps w1 */
    wuss_window_close(win_w3); /* autoclose: reaps w3 */
    mk_task_count = 0; /* all three tasks reaped themselves; drop stale entries */
  }

  printf("test: wuss_task_set_hidden hides a task and its children and reveals only what it hid\n");
  {
    static test_task_t tc_h, tc_hc;
    wuss_task_t       *parent_h, *child_h;
    box_t              box_h;
    wuss_window_t     *win_p, *win_c, *win_own;

    reap_test_tasks();
    memset(&tc_h, 0, sizeof(tc_h));
    memset(&tc_hc, 0, sizeof(tc_hc));
    parent_h = mk_task(wuss, test_handle, &tc_h);
    if (parent_h == NULL) goto Failure;
    wuss_set_task_parent(wuss, parent_h);
    child_h = mk_task(wuss, test_handle, &tc_hc);
    wuss_set_task_parent(wuss, NULL);
    if (child_h == NULL) goto Failure;

    box_h.x0 = 0;  box_h.y0 = 0;
    box_h.x1 = 40; box_h.y1 = 40;
    rc = wuss_window_create(parent_h, &box_h, "P", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_h), SIZE2D(0, 0), &win_p);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(child_h, &box_h, "C", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_h), SIZE2D(0, 0), &win_c);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(parent_h, &box_h, "O",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_HIDDEN,
                            wuss_NO_BACKDROP,
                            box_size(&box_h), SIZE2D(0, 0), &win_own);
    if (rc != result_OK) goto Failure;

    if (!wuss_task_has_windows(wuss, parent_h)) goto Failure;

    rc = wuss_task_set_hidden(wuss, parent_h, 1);
    if (rc != result_OK) goto Failure;
    if (!(win_p->flags & wuss_WINDOW_HIDDEN) ||
        !(win_c->flags & wuss_WINDOW_HIDDEN))
      goto Failure; /* parent and child both hidden */

    rc = wuss_task_set_hidden(wuss, parent_h, 0);
    if (rc != result_OK) goto Failure;
    if ((win_p->flags & wuss_WINDOW_HIDDEN) ||
        (win_c->flags & wuss_WINDOW_HIDDEN))
      goto Failure; /* both back */
    if (!(win_own->flags & wuss_WINDOW_HIDDEN))
      goto Failure; /* a window hidden in its own right stays so */

    wuss_window_close(win_p);
    wuss_window_close(win_c);
    if (wuss_task_has_windows(wuss, parent_h))
      goto Failure; /* only the window hidden in its own right is left */

    wuss_task_destroy(parent_h); /* takes child_h with it */
    mk_task_count = 0;
    rc = result_OK;
  }

  printf("test: app cycling, hide others and show all\n");
  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *app_a, *app_b;
    box_t              box_c;
    wuss_window_t     *a1, *a2, *b1;

    reap_test_tasks();
    memset(&tc_a, 0, sizeof(tc_a));
    memset(&tc_b, 0, sizeof(tc_b));
    app_a = mk_task(wuss, test_handle, &tc_a);
    app_b = mk_task(wuss, test_handle, &tc_b);
    if (app_a == NULL || app_b == NULL) goto Failure;

    box_c.x0 = 0;  box_c.y0 = 0;
    box_c.x1 = 40; box_c.y1 = 40;
    rc = wuss_window_create(app_a, &box_c, "A1",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            box_size(&box_c), SIZE2D(0, 0), &a1);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(app_a, &box_c, "A2",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            box_size(&box_c), SIZE2D(0, 0), &a2);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(app_b, &box_c, "B1",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            box_size(&box_c), SIZE2D(0, 0), &b1);
    if (rc != result_OK) goto Failure;

    /* front to back: b1 a2 a1 */
    if (!wuss_task_cycle_windows(wuss, app_a, 1) ||
        wuss_get_focus(wuss) != a1)
      goto Failure; /* rearmost window of A comes forward */
    if (!wuss_task_cycle_windows(wuss, app_a, -1) ||
        wuss_get_focus(wuss) != a2)
      goto Failure; /* front window of A goes back, the next comes forward */
    if (wuss_task_cycle_windows(wuss, app_b, 1))
      goto Failure; /* one window: nothing to cycle */

    /* front to back: a2 b1 a1 */
    if (!wuss_cycle_tasks(wuss, 1) || wuss_get_focus(wuss) != b1)
      goto Failure; /* rearmost other app comes forward */
    if (!wuss_cycle_tasks(wuss, -1) || wuss_get_focus(wuss) != a2)
      goto Failure; /* front app goes behind the rest */

    rc = wuss_task_hide_others(wuss, app_a);
    if (rc != result_OK) goto Failure;
    if (!(b1->flags & wuss_WINDOW_HIDDEN) ||
        (a1->flags & wuss_WINDOW_HIDDEN) || (a2->flags & wuss_WINDOW_HIDDEN))
      goto Failure;
    if (wuss_cycle_tasks(wuss, 1))
      goto Failure; /* a lone visible app */

    rc = wuss_task_hide_app(wuss, app_a);
    if (rc != result_OK) goto Failure;
    if (!(a1->flags & wuss_WINDOW_HIDDEN) || wuss_get_focus(wuss) != NULL)
      goto Failure;

    rc = wuss_task_show_all(wuss);
    if (rc != result_OK) goto Failure;
    if ((a1->flags & wuss_WINDOW_HIDDEN) || (b1->flags & wuss_WINDOW_HIDDEN) ||
        wuss_get_focus(wuss) == NULL)
      goto Failure;

    wuss_window_close(a1);
    wuss_window_close(a2);
    wuss_window_close(b1);
    reap_test_tasks();
    rc = result_OK;
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_task_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_task_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
