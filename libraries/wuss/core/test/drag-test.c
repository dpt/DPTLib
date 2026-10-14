/* core/test/drag-test.c -- wuss core drag and mouse capture tests */

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

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

result_t wuss_drag_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  test_task_t    tc_a, tc_b;
  wuss_task_t   *delegate_a, *delegate_b;
  box_t          box_a, box_b;
  wuss_window_t *win_a, *win_b;
  wuss_window_t *hit;

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: create windows A and B for the core drag tests\n");

  memset(&tc_a, 0, sizeof(tc_a));
  delegate_a = mk_task(wuss, test_handle, &tc_a);
  if (delegate_a == NULL) goto Failure;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(delegate_a, &box_a, "A", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, box_size(&box_a), SIZE2D(0, 0),
                          &win_a);
  if (rc != result_OK)
    goto Failure;

  memset(&tc_b, 0, sizeof(tc_b));
  delegate_b = mk_task(wuss, test_handle, &tc_b);
  if (delegate_b == NULL) goto Failure;

  box_b.x0 = 100;
  box_b.y0 = 100;
  box_b.x1 = 200;
  box_b.y1 = 200;
  rc = wuss_window_create(delegate_b, &box_b, "B", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, box_size(&box_b), SIZE2D(0, 0),
                          &win_b);
  if (rc != result_OK)
    goto Failure;

  printf("test: wuss_drag_start rejects a NULL window or a non-positive "
        "size\n");

  rc = wuss_drag_start(wuss, NULL, SIZE2D(8, 8), POINT(4, 4));
  if (rc != result_BAD_ARG)
    goto Failure;

  rc = wuss_drag_start(wuss, win_a, SIZE2D(0, 8), POINT(0, 0));
  if (rc != result_BAD_ARG)
    goto Failure;

  printf("test: a core drag takes over input -- MOUSE_UP ends it with "
        "wuss_EVENT_DRAG_END (drop = window under the pointer, not "
        "cancelled), and it is not delivered as a plain MOUSE_UP\n");

  tc_a.drag_end_count = 0;
  tc_a.mouse_count    = 0;
  tc_b.mouse_count    = 0;

  rc = wuss_drag_start(wuss, win_a, SIZE2D(8, 8), POINT(4, 4));
  if (rc != result_OK)
    goto Failure;
  if (!wuss_is_dragging(wuss))
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(120, 120), &hit); /* over B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure; /* hit-test still reported, even though nothing was delivered */
  if (tc_b.mouse_count != 0)
    goto Failure; /* moves during a drag are not delivered as wuss_EVENT_MOUSE */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (wuss_is_dragging(wuss))
    goto Failure;
  if (tc_b.mouse_count != 0)
    goto Failure; /* the ending MOUSE_UP is consumed by the drag, not B */
  if (tc_a.drag_end_count != 1)
    goto Failure;
  if (tc_a.last_drag_end_drop != win_b)
    goto Failure;
  if (tc_a.last_drag_end_cancelled)
    goto Failure;
  if (tc_a.last_drag_end_x != 120 || tc_a.last_drag_end_y != 120)
    goto Failure;

  printf("test: Escape cancels an active core drag -- wuss_EVENT_DRAG_END "
        "with drop = NULL and cancelled = 1\n");

  tc_a.drag_end_count = 0;

  rc = wuss_drag_start(wuss, win_a, SIZE2D(8, 8), POINT(4, 4));
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(120, 120), NULL); /* over B, cancelled anyway */
  if (rc != result_OK)
    goto Failure;

  {
    int claimed;

    rc = wuss_key(wuss, wuss_KEY_ESCAPE, wuss_KEY_MOD_NONE, &claimed);
    if (rc != result_OK)
      goto Failure;
    if (!claimed)
      goto Failure;
  }

  if (wuss_is_dragging(wuss))
    goto Failure;
  if (tc_a.drag_end_count != 1)
    goto Failure;
  if (tc_a.last_drag_end_drop != NULL)
    goto Failure;
  if (!tc_a.last_drag_end_cancelled)
    goto Failure;

  printf("test: wuss_idle advances the marching-ants frame counter while a "
        "drag is active, and stops once it ends\n");

  rc = wuss_drag_start(wuss, win_a, SIZE2D(8, 8), POINT(4, 4));
  if (rc != result_OK)
    goto Failure;

  rc = wuss_idle(wuss);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_idle(wuss);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(50, 50), wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_idle(wuss); /* no drag active: must not misbehave */
  if (rc != result_OK)
    goto Failure;

  printf("test: closing a drag's source window abandons the drag, so a "
        "later MOUSE_UP does not deliver DRAG_END to the freed window\n");

  {
    wuss_window_t *win_c;

    rc = wuss_window_create(delegate_a, &box_b, "C", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, box_size(&box_b), SIZE2D(0, 0),
                            &win_c);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_drag_start(wuss, win_c, SIZE2D(8, 8), POINT(4, 4));
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_c);
    if (wuss_is_dragging(wuss))
      goto Failure;

    tc_a.drag_end_count = 0;

    rc = wuss_mouse_click(wuss, POINT(50, 50), wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (rc != result_OK)
      goto Failure;
    if (tc_a.drag_end_count != 0)
      goto Failure;
  }

  printf("test: a DRAGGABLE icon starts a core drag once the pointer clears "
        "drag_threshold_px from the press, sized to its bbox\n");

  {
    wuss_icon_spec_t spec;
    wuss_icon_t     *draggable;
    wuss_config_t    dconfig;
    int              drag_end_before;

    memset(&spec, 0, sizeof(spec));
    spec.bbox = (box_t) BOX_POS_SIZE(10, 10, 20, 20);
    spec.type = wuss_ICON_TYPE_DRAGGABLE;
    rc = wuss_icon_create(win_a, &spec, &draggable);
    if (rc != result_OK)
      goto Failure;

    wuss_get_config(wuss, &dconfig);
    drag_end_before = tc_a.drag_end_count;

    /* content top-left is screen (1, 21): 1px outline, 20px titlebar above
     * the default furniture -- bbox (10,10)-(30,30) local puts its top-left
     * at screen (11, 31) */
    rc = wuss_mouse_click(wuss, POINT(11, 31), wuss_BUTTON_SELECT,
                          wuss_MOUSE_DOWN, NULL);
    if (rc != result_OK)
      goto Failure;
    if (wuss_is_dragging(wuss))
      goto Failure; /* no movement yet: still an ordinary press */

    /* short of the threshold: stays an ordinary press */
    rc = wuss_mouse_move(wuss, POINT(11 + dconfig.mouse.drag_threshold_px - 1, 31),
                         NULL);
    if (rc != result_OK)
      goto Failure;
    if (wuss_is_dragging(wuss))
      goto Failure;

    /* clears the threshold: becomes a core drag, released as an icon press */
    rc = wuss_mouse_move(wuss, POINT(11 + dconfig.mouse.drag_threshold_px + 1, 31),
                         NULL);
    if (rc != result_OK)
      goto Failure;
    if (!wuss_is_dragging(wuss))
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(11 + dconfig.mouse.drag_threshold_px + 1, 31),
                          wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (rc != result_OK)
      goto Failure;
    if (wuss_is_dragging(wuss))
      goto Failure;
    if (tc_a.drag_end_count != drag_end_before + 1)
      goto Failure;

    wuss_icon_delete(win_a, draggable);
  }

  printf("test: a Select press on plain content captures the mouse -- moves "
        "and the release go to that window even over another window, and the "
        "release ends the capture\n");

  tc_a.mouse_count = 0;
  tc_b.mouse_count = 0;

  rc = wuss_mouse_click(wuss, POINT(50, 50), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, NULL);
  if (rc != result_OK)
    goto Failure;
  if (!wuss_is_capturing(wuss))
    goto Failure;
  if (tc_a.mouse_count != 1)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(150, 150), &hit); /* over B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 2 || tc_a.last_action != wuss_MOUSE_MOVE)
    goto Failure;
  if (tc_a.last_x < 100)
    goto Failure; /* a point outside A's own content box */
  if (tc_b.mouse_count != 0)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(150, 150), wuss_BUTTON_SELECT,
                        wuss_MOUSE_UP, NULL);
  if (rc != result_OK)
    goto Failure;
  if (wuss_is_capturing(wuss))
    goto Failure;
  if (tc_a.mouse_count != 3 || tc_a.last_action != wuss_MOUSE_UP)
    goto Failure;
  if (tc_b.mouse_count != 0)
    goto Failure;

  printf("test: starting a core drag from a captured press hands the pointer "
         "to the drag, so the ants follow it and the release ends it\n");

  rc = wuss_mouse_click(wuss, POINT(50, 50), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, NULL);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_drag_start(wuss, win_a, SIZE2D(8, 8), POINT(4, 4));
  if (rc != result_OK)
    goto Failure;
  if (wuss_is_capturing(wuss) || !wuss_is_dragging(wuss))
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(60, 60), wuss_BUTTON_SELECT,
                        wuss_MOUSE_UP, NULL);
  if (rc != result_OK)
    goto Failure;
  if (wuss_is_capturing(wuss) || wuss_is_dragging(wuss))
    goto Failure;

  printf("test: closing the capturing window ends the capture\n");

  rc = wuss_mouse_click(wuss, POINT(50, 50), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, NULL);
  if (rc != result_OK)
    goto Failure;
  if (!wuss_is_capturing(wuss))
    goto Failure;

  wuss_window_close(win_a);
  if (wuss_is_capturing(wuss))
    goto Failure;

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_drag_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_drag_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
