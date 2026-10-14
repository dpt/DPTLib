/* core/test/content-test.c -- wuss content space tests */

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

result_t wuss_content_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  wuss_window_t *hit;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: mouse and scroll events arrive in virtual content space, with the scroll offset applied exactly once\n");

  {
    static test_task_t tc_s = { 0 };
    wuss_task_t       *delegate_s;
    box_t              box_s, content_s;
    wuss_window_t     *win_s;
    point_t            scroll;

    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 60; box_s.y1 = 60; /* 50x50 content onto a 200x200 doc: room to scroll */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    scroll.x = 30;
    scroll.y = 40;
    wuss_window_set_scroll(win_s, scroll);
    wuss_window_get_scroll(win_s, &scroll); /* read back in case it clamped */

    wuss_window_get_content_bounds(win_s, &content_s);

    rc = wuss_mouse_click(wuss,
                          POINT(content_s.x0 + 5, content_s.y0 + 7),
                          wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;
    if (tc_s.last_x != 5 + scroll.x || tc_s.last_y != 7 + scroll.y)
      goto Failure; /* a task adding the scroll offset itself would double-count it */

    rc = wuss_mouse_click(wuss,
                          POINT(content_s.x0 + 5, content_s.y0 + 7),
                          wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss,
                         POINT(content_s.x0 + 11, content_s.y0 + 13),
                         &hit);
    if (rc != result_OK)
      goto Failure;
    if (tc_s.last_x != 11 + scroll.x || tc_s.last_y != 13 + scroll.y)
      goto Failure;

    tc_s.last_scroll_x = -1;
    tc_s.last_scroll_y = -1;
    rc = wuss_scroll(wuss,
                     POINT(content_s.x0 + 3, content_s.y0 + 4),
                     1, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;
    if (tc_s.last_scroll_x != 3 + scroll.x || tc_s.last_scroll_y != 4 + scroll.y)
      goto Failure;

    wuss_window_close(win_s);
  }

  printf("test: content bounds survive furniture, including the interior rules\n");

  {
    /* Furniture -- outline, titlebar, scrollbars and the rules dividing the
     * content from them -- is added outside the requested content box, never
     * carved out of it, so what the caller asks for is what it gets, both at
     * creation and after a resize. */
    static test_task_t tc_r;
    wuss_task_t       *delegate_r;
    box_t              box_r, content_r;
    wuss_window_t     *win_r;

    memset(&tc_r, 0, sizeof(tc_r));
    delegate_r = mk_task(wuss, test_handle, &tc_r);
    if (delegate_r == NULL) goto Failure;

    box_r.x0 = 20;
    box_r.y0 = 30;
    box_r.x1 = 120;
    box_r.y1 = 110;
    rc = wuss_window_create(delegate_r,
                            &box_r,
                            "rules",
                            wuss_WINDOW_DEFAULT, /* all furniture present */
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400),
                            SIZE2D(0, 0),
                            &win_r);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_r, &content_r);
    if (content_r.x1 - content_r.x0 != box_r.x1 - box_r.x0 ||
        content_r.y1 - content_r.y0 != box_r.y1 - box_r.y0)
      goto Failure;

    rc = wuss_window_resize(win_r, SIZE2D(61, 47));
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_r, &content_r);
    if (content_r.x1 - content_r.x0 != 61 || content_r.y1 - content_r.y0 != 47)
      goto Failure;

    wuss_window_close(win_r);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_content_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_content_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
