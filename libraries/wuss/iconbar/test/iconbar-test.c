/* iconbar/test/iconbar-test.c -- wuss icon bar tests */

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
#include "../../iconbar.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONBAR)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

result_t wuss_iconbar_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: icon bar is a real, occludable, pinned window\n");

  {
    wuss_iconbar_icon_spec_t spec;
    wuss_iconbar_icon_t     *icon;
    wuss_task_t             *owner;
    test_task_t              tc_bar;
    box_t                    box_over, bar_before;
    wuss_window_t           *win_over;
    result_t                 mrc;

    memset(&tc_bar, 0, sizeof(tc_bar));
    owner = mk_task(wuss, test_handle, &tc_bar);
    if (owner == NULL) goto Failure;

    memset(&spec, 0, sizeof(spec));
    spec.text = "T";

    rc = wuss_iconbar_icon_create(wuss, owner, &spec, &icon);
    if (rc != result_OK || icon == NULL)
      goto Failure;

    /* first icon creates the bar's window lazily, pinned to the bottom of
     * the 200x200 test screen and full width */
    if (wuss->iconbar.window == NULL)
      goto Failure;
    if (wuss->iconbar.window->visible.x0 != 0 ||
        wuss->iconbar.window->visible.x1 != 200 ||
        wuss->iconbar.window->visible.y1 != 200 ||
        wuss->iconbar.window->visible.y0 != 200 - WUSS_ICONBAR_HEIGHT)
      goto Failure;

    /* icons are as wide as their label, WUSS_ICONBAR_GAP apart, and pack
     * from the edge of their side */
    {
      box_t                b0, b1, b2;
      wuss_iconbar_icon_t *ic;

      spec.text = "Left";
      rc = wuss_iconbar_icon_create(wuss, owner, &spec, &ic);
      if (rc != result_OK)
        goto Failure;
      spec.text = "Right";
      spec.side = wuss_ICONBAR_RIGHT;
      rc = wuss_iconbar_icon_create(wuss, owner, &spec, &ic);
      if (rc != result_OK)
        goto Failure;
      spec.side = wuss_ICONBAR_LEFT;

      wuss__iconbar_slot_box(wuss, 0, &b0);
      wuss__iconbar_slot_box(wuss, 1, &b1);
      wuss__iconbar_slot_box(wuss, 2, &b2);
      if (b0.x0 != WUSS_ICONBAR_GAP || b0.x1 - b0.x0 < 1 ||
          b1.x0 != b0.x1 + WUSS_ICONBAR_GAP ||
          b2.x1 != 200 - WUSS_ICONBAR_GAP)
        goto Failure;

      wuss_iconbar_icon_destroy(wuss, wuss->iconbar.icons[2]);
      wuss_iconbar_icon_destroy(wuss, wuss->iconbar.icons[1]);
    }

    /* an overfull bar scrolls while the pointer hovers near an end */
    {
      box_t                bar, s0;
      wuss_iconbar_icon_t *ic;
      int                  i, y, x0;

      spec.text = "Wide label";
      for (i = 0; i < 6; i++)
      {
        rc = wuss_iconbar_icon_create(wuss, owner, &spec, &ic);
        if (rc != result_OK)
          goto Failure;
      }

      wuss__iconbar_box(wuss, &bar);
      y = bar.y0 + 5;
      wuss__iconbar_slot_box(wuss, 0, &s0);
      x0 = s0.x0;

      /* pointer mid-bar: no scroll */
      wuss->pointer = POINT(100, y);
      (void) wuss_idle(wuss);
      if (wuss->iconbar.scroll != 0)
        goto Failure;

      /* right end: scrolls left, icon 0 moves left of where it was */
      wuss->pointer = POINT(bar.x1 - 1, y);
      (void) wuss_idle(wuss);
      wuss__iconbar_slot_box(wuss, 0, &s0);
      if (wuss->iconbar.scroll != WUSS_ICONBAR_SCROLL_STEP ||
          s0.x0 != x0 - WUSS_ICONBAR_SCROLL_STEP)
        goto Failure;

      /* held long enough, it clamps at the strip's end */
      for (i = 0; i < 200; i++)
        (void) wuss_idle(wuss);
      i = wuss->iconbar.scroll;
      (void) wuss_idle(wuss);
      if (i <= WUSS_ICONBAR_SCROLL_STEP || wuss->iconbar.scroll != i)
        goto Failure;

      /* left end: scrolls back to 0 */
      wuss->pointer = POINT(0, y);
      for (i = 0; i < 200; i++)
        (void) wuss_idle(wuss);
      if (wuss->iconbar.scroll != 0)
        goto Failure;

      for (i = 0; i < 6; i++)
        wuss_iconbar_icon_destroy(wuss, wuss->iconbar.icons[1]);
      (void) wuss_idle(wuss);
      if (wuss->iconbar.n != 1 || wuss->iconbar.scroll != 0)
        goto Failure;
    }

    /* a real z-order member can be moved/resized like any other window --
     * except this one is pinned, so both must refuse */
    wuss_window_move(wuss->iconbar.window, POINT(5, 5));
    if (wuss->iconbar.window->visible.x0 != 0 ||
        wuss->iconbar.window->visible.y0 != 200 - WUSS_ICONBAR_HEIGHT)
      goto Failure; /* move silently ignored on a pinned window */

    mrc = wuss_window_resize(wuss->iconbar.window, SIZE2D(50, 50));
    if (mrc != result_WUSS_PINNED)
      goto Failure;

    /* a window created after the bar, overlapping its strip, must occlude
     * it in z-order -- covered by the general occlusion machinery now that
     * the bar is a normal window, not a special-cased always-on-top draw */
    box_over.x0 = 0;   box_over.y0 = 100;
    box_over.x1 = 200; box_over.y1 = 200 - WUSS_ICONBAR_HEIGHT + 10;
    rc = wuss_window_create(mk_task(wuss, paint_handle, NULL), &box_over,
                            "OVER", wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(200, 50), SIZE2D(20, 20), &win_over);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* a click on the strip of the bar covered by win_over must reach
     * win_over, not the icon underneath it */
    {
      wuss_window_t *hit;

      rc = wuss_mouse_click(wuss, POINT(10, 200 - WUSS_ICONBAR_HEIGHT + 5),
                            wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
      if (rc != result_OK || hit != win_over)
        goto Failure;
      rc = wuss_mouse_click(wuss, POINT(10, 200 - WUSS_ICONBAR_HEIGHT + 5),
                            wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
      if (rc != result_OK)
        goto Failure;
    }

    /* a click on the icon itself (below win_over's strip) reaches it --
     * past the bar's WUSS_ICONBAR_GAP leading gap */
    tc_bar.icon_count = 0;
    rc = wuss_mouse_click(wuss, POINT(WUSS_ICONBAR_GAP + 1, 199),
                          wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(WUSS_ICONBAR_GAP + 1, 199),
                          wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (rc != result_OK || tc_bar.icon_count == 0)
      goto Failure;

    wuss_window_close(win_over);

    /* a screen resize keeps the bar pinned to the new bottom edge rather
     * than nudging/shrinking it like an ordinary window */
    bar_before = wuss->iconbar.window->visible;
    rc = wuss_resize(wuss, &fixture.scr);
    if (rc != result_OK)
      goto Failure;
    if (wuss->iconbar.window->visible.x0 != bar_before.x0 ||
        wuss->iconbar.window->visible.y0 != bar_before.y0 ||
        wuss->iconbar.window->visible.x1 != bar_before.x1 ||
        wuss->iconbar.window->visible.y1 != bar_before.y1)
      goto Failure; /* same screen size in -> same pinned box out */

    wuss_iconbar_icon_destroy(wuss, icon);
    reap_test_tasks();
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_iconbar_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_iconbar_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
