/* core/test/focus-test.c -- wuss input focus tests */

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

result_t wuss_focus_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: input focus: flag gate, click-to-focus, key routing, loss on hide/close\n");

  {
    static test_task_t tc_f, tc_n;
    wuss_task_t       *delegate_f, *delegate_n;
    box_t              box_f, box_g, box_n, content_f, content_n;
    wuss_window_t     *win_f, *win_g, *win_n;
    int                claimed;

    delegate_f = mk_task(wuss, test_handle, &tc_f);
    delegate_n = mk_task(wuss, test_handle, &tc_n);
    if (delegate_f == NULL || delegate_n == NULL) goto Failure;

    box_f.x0 = 10;  box_f.y0 = 30;  box_f.x1 = 60;  box_f.y1 = 80;
    box_g.x0 = 70;  box_g.y0 = 30;  box_g.x1 = 120; box_g.y1 = 80;
    box_n.x0 = 130; box_n.y0 = 30;  box_n.x1 = 180; box_n.y1 = 80;
    rc = wuss_window_create(delegate_f, &box_f, "F",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_f);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(delegate_f, &box_g, "G",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_g);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_create(delegate_n, &box_n, "N", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_n);
    if (rc != result_OK) goto Failure;

    wuss_window_get_content_bounds(win_f, &content_f);
    wuss_window_get_content_bounds(win_n, &content_n);

    rc = wuss_redraw_dirty(wuss); /* flush the creates' own invalidates */
    if (rc != result_OK) goto Failure;

    /* nothing focused: keys go unclaimed */
    if (wuss_get_focus(wuss) != NULL) goto Failure;
    rc = wuss_key(wuss, 'a', wuss_KEY_MOD_NONE, &claimed);
    if (rc != result_OK || claimed) goto Failure;

    /* the flag gates wuss_set_focus */
    if (wuss_set_focus(wuss, win_n) != result_BAD_ARG) goto Failure;

    /* a Menu press never takes focus; a Select press on content does, and
     * GAIN_FOCUS arrives */
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_MENU, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_MENU, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != NULL) goto Failure;
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_f.x0 + 5, content_f.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != win_f || tc_f.gain_focus_count != 1)
      goto Failure;

    /* the focused titlebar is repainted in the tint */
    if (wuss_get_dirty_count(wuss) == 0) goto Failure;

    /* keys reach the focused window, with modifiers */
    rc = wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_SHIFT, &claimed);
    if (rc != result_OK || !claimed || tc_f.key_count != 1 ||
        tc_f.last_key != wuss_KEY_LEFT ||
        tc_f.last_key_mods != wuss_KEY_MOD_SHIFT)
      goto Failure;

    /* a declined key is reported unclaimed */
    tc_f.unclaim_keys = 1;
    rc = wuss_key(wuss, 'x', wuss_KEY_MOD_CTRL, &claimed);
    if (rc != result_OK || claimed || tc_f.key_count != 2) goto Failure;
    tc_f.unclaim_keys = 0;

    /* clicking a non-focusable window leaves the focus alone */
    wuss_mouse_click(wuss, POINT(content_n.x0 + 5, content_n.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_n.x0 + 5, content_n.y0 + 5),
                     wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != win_f || tc_n.key_count != 0) goto Failure;

    /* refocusing the holder fires nothing */
    if (wuss_set_focus(wuss, win_f) != result_OK ||
        tc_f.gain_focus_count != 1 || tc_f.lose_focus_count != 0)
      goto Failure;

    /* moving focus: LOSE then GAIN */
    if (wuss_set_focus(wuss, win_g) != result_OK ||
        tc_f.lose_focus_count != 1 || tc_f.gain_focus_count != 2)
      goto Failure;

    /* hiding the focused window drops focus with LOSE; a hidden window
     * can't be focused */
    rc = wuss_window_set_hidden(win_g, 1);
    if (rc != result_OK || wuss_get_focus(wuss) != NULL ||
        tc_f.lose_focus_count != 2)
      goto Failure;
    if (wuss_set_focus(wuss, win_g) != result_BAD_ARG) goto Failure;

    /* try_close fires LOSE; forced close clears silently */
    if (wuss_set_focus(wuss, win_f) != result_OK) goto Failure;
    rc = wuss_window_try_close(win_f);
    if (rc != result_OK || wuss_get_focus(wuss) != NULL ||
        tc_f.lose_focus_count != 3)
      goto Failure;

    rc = wuss_window_set_hidden(win_g, 0);
    if (rc != result_OK) goto Failure;
    if (wuss_set_focus(wuss, win_g) != result_OK) goto Failure;
    wuss_window_close(win_g);
    if (wuss_get_focus(wuss) != NULL || tc_f.lose_focus_count != 3)
      goto Failure;

    wuss_window_close(win_n);
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
  }

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)
#endif

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_focus_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_focus_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
