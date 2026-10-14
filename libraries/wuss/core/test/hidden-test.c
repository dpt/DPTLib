/* core/test/hidden-test.c -- wuss hidden window and PRE_SHOW tests */

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
#include "framebuf/screen.h"
#include "wuss/icon-spec.h"
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

result_t wuss_hidden_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  wuss_window_t *hit;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: closing a hidden window never dirties the visible window sitting under its old footprint\n");

  {
    static test_task_t tc_c;
    wuss_task_t       *delegate_c;
    box_t              box_visible, box_hidden, region;
    wuss_window_t     *win_visible, *win_hidden_c;
    int                i, overlap_x, overlap_y, overlap_dirty;

    tc_c.redraw_count = 0;
    tc_c.mouse_count  = 0;
    delegate_c = mk_task(wuss, test_handle, &tc_c);
    if (delegate_c == NULL) goto Failure;

    /* a visible window occupying the screen's top-left, matching where a
     * hidden singleton dialogue (e.g. wuss_proginfo_window) is always
     * created (content box (0,0)-(w,h): see wuss_info_create) */
    box_visible.x0 = 0; box_visible.y0 = 0;
    box_visible.x1 = 80; box_visible.y1 = 80;
    rc = wuss_window_create(delegate_c, &box_visible, "V", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_visible);
    if (rc != result_OK)
      goto Failure;

    /* hidden window whose footprint overlaps win_visible's -- never drawn,
     * so closing it must not touch pixels that belong to win_visible */
    box_hidden.x0 = 0; box_hidden.y0 = 0;
    box_hidden.x1 = 40; box_hidden.y1 = 40;
    rc = wuss_window_create(delegate_c, &box_hidden, "Hidden",
                            wuss_WINDOW_HIDDEN, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_hidden_c);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates' own invalidates */
    if (rc != result_OK)
      goto Failure;

    overlap_x = (box_hidden.x0 + box_hidden.x1) / 2;
    overlap_y = (box_hidden.y0 + box_hidden.y1) / 2;

    wuss_window_close(win_hidden_c);

    overlap_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, overlap_x, overlap_y))
        overlap_dirty = 1;
    }
    if (overlap_dirty)
      goto Failure; /* closing a hidden window must not force a repaint of
                      * whatever visible window/backdrop actually occupies
                      * its old, never-drawn footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_visible);
  }

  printf("test: wuss_window_set_hidden fires PRE_SHOW; not opting in still shows the window\n");
  {
    static test_task_t tc_ps;
    wuss_task_t       *delegate_ps;
    box_t              box_ps;
    wuss_window_t     *win_ps;

    memset(&tc_ps, 0, sizeof(tc_ps));
    delegate_ps = mk_task(wuss, test_handle, &tc_ps);
    if (delegate_ps == NULL)
      goto Failure;

    box_ps.x0 = 0;  box_ps.y0 = 0;
    box_ps.x1 = 60; box_ps.y1 = 60;
    rc = wuss_window_create(delegate_ps, &box_ps, "PS", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_ps), SIZE2D(0, 0), &win_ps);
    if (rc != result_OK)
      goto Failure;

    /* hide it: no event, and it must not catch the pointer any more */
    rc = wuss_window_set_hidden(win_ps, 1);
    if (rc != result_OK || tc_ps.pre_show_count != 0 || tc_ps.show_count != 0)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit == win_ps)
      goto Failure; /* hidden window is not hit-tested */
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);

    /* handler does not call wuss_window_reveal_now: PRE_SHOW still fires,
     * but the window shows anyway (default proceed) */
    tc_ps.veto_pre_show = 1;
    rc = wuss_window_set_hidden(win_ps, 0);
    if (rc != result_OK)
      goto Failure;
    if (tc_ps.pre_show_count != 1 || tc_ps.show_count != 1)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_ps)
      goto Failure;
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);
    rc = wuss_window_set_hidden(win_ps, 1);
    if (rc != result_OK)
      goto Failure;

    /* handler calls wuss_window_reveal_now explicitly: same result */
    tc_ps.veto_pre_show = 0;
    rc = wuss_window_set_hidden(win_ps, 0);
    if (rc != result_OK)
      goto Failure;
    if (tc_ps.pre_show_count != 2 || tc_ps.show_count != 2)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_ps)
      goto Failure;
    (void) wuss_mouse_click(wuss, POINT(10, 10), wuss_BUTTON_SELECT,
                            wuss_MOUSE_UP, &hit);

    wuss_task_destroy(delegate_ps);
    mk_task_count = 0; /* delegate_ps is gone; drop the stale registry entry */
    rc = result_OK;
  }

  printf("test: a plain window's PRE_SHOW carries a NULL handle -- calling "
         "wuss_menu_open_window_now only when handle is non-NULL does not "
         "crash (regression: saturn.c/tasks.c called it unconditionally)\n");
  {
    static test_task_t tc_psh;
    wuss_task_t       *delegate_psh;
    box_t              box_psh;
    wuss_window_t     *win_psh;

    memset(&tc_psh, 0, sizeof(tc_psh));
    tc_psh.open_window_via_handle = 1;
    delegate_psh = mk_task(wuss, test_handle, &tc_psh);
    if (delegate_psh == NULL)
      goto Failure;

    box_psh.x0 = 0;  box_psh.y0 = 0;
    box_psh.x1 = 60; box_psh.y1 = 60;
    rc = wuss_window_create(delegate_psh, &box_psh, "PSH", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            box_size(&box_psh), SIZE2D(0, 0), &win_psh);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_window_set_hidden(win_psh, 1);
    if (rc != result_OK)
      goto Failure;

    /* handle == NULL for a plain window; the handler's NULL guard must
     * take the early return and the window still shows (already
     * defaulted to proceed before the handler ran) */
    rc = wuss_window_set_hidden(win_psh, 0);
    if (rc != result_OK || tc_psh.pre_show_count != 1)
      goto Failure;
    if (win_psh->flags & wuss_WINDOW_HIDDEN)
      goto Failure;

    wuss_task_destroy(delegate_psh);
    mk_task_count = 0;
    rc = result_OK;
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_hidden_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_hidden_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
