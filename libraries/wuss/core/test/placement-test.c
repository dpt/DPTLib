/* core/test/placement-test.c -- wuss window placement tests */

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

result_t wuss_placement_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  wuss_window_t *hit;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_window_create_placed tiles windows and reclaims a closed slot\n");

  {
    /* Windows created without a position are packed towards the top-left and
     * must not overlap; closing one frees its slot for the next create. */
    static test_task_t tc_p[4];
    wuss_task_t       *delegate_p;
    wuss_window_t     *win_p[4];
    box_t              vis[4], probe;
    int                k, m;

    /* placement avoids every shown window, so clear out windows left open
     * by earlier tests */
    reap_test_tasks();

    memset(tc_p, 0, sizeof(tc_p));
    delegate_p = mk_task(wuss, test_handle, &tc_p[0]);
    if (delegate_p == NULL) goto Failure;

    for (k = 0; k < 4; k++)
    {
      rc = wuss_window_create_placed(delegate_p,
                                     SIZE2D(40, 30),
                                     "P",
                                     wuss_WINDOW_DEFAULT,
                                     wuss_NO_BACKDROP,
                                     SIZE2D(40, 30),
                                     SIZE2D(0, 0),
                                     &win_p[k]);
      if (rc != result_OK)
        goto Failure;
      wuss_window_get_visible_bounds(win_p[k], &vis[k]);
    }

    for (k = 0; k < 4; k++)
      for (m = k + 1; m < 4; m++)
        if (box_intersects(&vis[k], &vis[m]))
          goto Failure; /* auto-placed windows overlapped */

    /* free the second window's slot, then a new placed window should land
     * back in it rather than being pushed past the others */
    probe = vis[1];
    wuss_window_close(win_p[1]);

    rc = wuss_window_create_placed(delegate_p,
                                   SIZE2D(40, 30),
                                   "P",
                                   wuss_WINDOW_DEFAULT,
                                   wuss_NO_BACKDROP,
                                   SIZE2D(40, 30),
                                   SIZE2D(0, 0),
                                   &win_p[1]);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_p[1], &vis[1]);
    if (vis[1].x0 != probe.x0 || vis[1].y0 != probe.y0 ||
        vis[1].x1 != probe.x1 || vis[1].y1 != probe.y1)
      goto Failure; /* freed slot not reused */

    /* placement honours where windows actually are: free the second slot
     * again, drag the first window into it, and the next placed window must
     * avoid the moved window rather than land in the stale free slot */
    wuss_window_close(win_p[1]);
    wuss_window_move(win_p[0], POINT(probe.x0 + 5, probe.y0 + 20));
    wuss_window_get_visible_bounds(win_p[0], &vis[0]);

    rc = wuss_window_create_placed(delegate_p,
                                   SIZE2D(40, 30),
                                   "P",
                                   wuss_WINDOW_DEFAULT,
                                   wuss_NO_BACKDROP,
                                   SIZE2D(40, 30),
                                   SIZE2D(0, 0),
                                   &win_p[1]);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_p[1], &vis[1]);
    if (box_intersects(&vis[0], &vis[1]))
      goto Failure; /* placed over a moved window */

    for (k = 0; k < 4; k++)
      wuss_window_close(win_p[k]);
  }

  printf("test: wuss_window_create_at anchors, places relative and avoids "
         "the icon bar\n");

  {
    static test_task_t tc_a;
    wuss_task_t       *delegate_a;
    wuss_place_t       place;
    wuss_window_t     *win_a, *win_b;
    box_t              vis_a, vis_b;
    int                bottom;
#ifdef WUSS_ICONBAR
    wuss_iconbar_icon_spec_t spec;
    wuss_iconbar_icon_t     *icon;
#endif

    reap_test_tasks();

    memset(&tc_a, 0, sizeof(tc_a));
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    /* top right: flush with the gutter on both edges it is against */
    memset(&place, 0, sizeof(place));
    place.align = wuss_ALIGN_TOP | wuss_ALIGN_RIGHT;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "A",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_a);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_a, &vis_a);
    if (vis_a.y0 != WUSS_PLACE_GUTTER || vis_a.x1 != 200 - WUSS_PLACE_GUTTER)
      goto Failure;

    /* relative: the footprint's top-left is offset from the other window's */
    memset(&place, 0, sizeof(place));
    place.relative_to = win_a;
    place.offset      = POINT(-32, 32);
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != vis_a.x0 - 32 || vis_b.y0 != vis_a.y0 + 32)
      goto Failure;
    wuss_window_close(win_b);

    /* relative past the bottom-right corner: clamped back onto the screen */
    place.offset = POINT(500, 500);
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x1 != 200 || vis_b.y1 != 200)
      goto Failure;
    wuss_window_close(win_b);

    /* centred (zero-initialised) */
    memset(&place, 0, sizeof(place));
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != (200 - (vis_b.x1 - vis_b.x0)) / 2 ||
        vis_b.y0 != (200 - (vis_b.y1 - vis_b.y0)) / 2)
      goto Failure;
    wuss_window_close(win_b);

    /* bottom left, avoiding the icon bar: sits a gutter above the bar */
    bottom = 200;
#ifdef WUSS_ICONBAR
    memset(&spec, 0, sizeof(spec));
    spec.text = "T";
    rc = wuss_iconbar_icon_create(wuss, delegate_a, &spec, &icon);
    if (rc != result_OK)
      goto Failure;
    bottom = 200 - WUSS_ICONBAR_HEIGHT;
#endif
    memset(&place, 0, sizeof(place));
    place.flags = wuss_PLACE_AVOID_ICONBAR;
    place.align = wuss_ALIGN_BOTTOM | wuss_ALIGN_LEFT;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != WUSS_PLACE_GUTTER ||
        vis_b.y1 != bottom - WUSS_PLACE_GUTTER)
      goto Failure;
    wuss_window_close(win_b);

    /* spare space first, searched from the anchor's corner: lands in the
     * free bottom-left, clear of A */
    place.flags |= wuss_PLACE_SPARE;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != WUSS_PLACE_GUTTER ||
        vis_b.y1 != bottom - WUSS_PLACE_GUTTER ||
        box_intersects(&vis_a, &vis_b))
      goto Failure;
    wuss_window_close(win_b);

    /* no anchor: searched from the top-left, as before */
    place.align = wuss_ALIGN_CENTRE;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 != WUSS_PLACE_GUTTER || vis_b.y0 != WUSS_PLACE_GUTTER ||
        box_intersects(&vis_a, &vis_b))
      goto Failure;
    wuss_window_close(win_b);

    /* bottom right: A leaves no room against the right edge, so nearest
     * that corner is the bottom row, clear of A, not the top-left */
    place.align = wuss_ALIGN_BOTTOM | wuss_ALIGN_RIGHT;
    rc = wuss_window_create_at(delegate_a, &place, SIZE2D(40, 30), "B",
                               wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                               SIZE2D(40, 30), SIZE2D(0, 0), &win_b);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_b, &vis_b);
    if (vis_b.x0 <= WUSS_PLACE_GUTTER ||
        vis_b.y1 != bottom - WUSS_PLACE_GUTTER ||
        box_intersects(&vis_a, &vis_b))
      goto Failure;
    wuss_window_close(win_b);

#ifdef WUSS_ICONBAR
    wuss_iconbar_icon_destroy(wuss, icon);
#endif
    reap_test_tasks();
  }

  printf("test: a dragged window snaps flush to a screen edge it comes "
         "within WUSS_SNAP_PX of\n");

  {
    static test_task_t tc_sn;
    wuss_task_t       *delegate_sn;
    box_t              box_sn, start, visible;
    wuss_window_t     *win_sn;
    point_t            grab;
    size2d_t           scr;

    delegate_sn = mk_task(wuss, test_handle, &tc_sn);
    if (delegate_sn == NULL) goto Failure;

    box_sn.x0 = 40; box_sn.y0 = 40;
    box_sn.x1 = 100; box_sn.y1 = 100;
    rc = wuss_window_create(delegate_sn, &box_sn, "SN", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_sn);
    if (rc != result_OK) goto Failure;

    scr = wuss->scr->size;
    wuss_window_get_visible_bounds(win_sn, &start);
    grab = POINT((start.x0 + start.x1) / 2, start.y0 + 3); /* titlebar */

    rc = wuss_mouse_click(wuss, grab, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_sn) goto Failure;

    /* within range of the top-left corner: snaps to both edges */
    wuss_mouse_move(wuss, POINT(grab.x - start.x0 + WUSS_SNAP_PX - 1, grab.y - start.y0 + WUSS_SNAP_PX - 1), NULL);
    wuss_window_get_visible_bounds(win_sn, &visible);
    if (visible.x0 != 0 || visible.y0 != 0) goto Failure;

    /* just out of range: follows the pointer */
    wuss_mouse_move(wuss, POINT(grab.x - start.x0 + WUSS_SNAP_PX, grab.y - start.y0 + WUSS_SNAP_PX), NULL);
    wuss_window_get_visible_bounds(win_sn, &visible);
    if (visible.x0 != WUSS_SNAP_PX || visible.y0 != WUSS_SNAP_PX) goto Failure;

    /* within range of the bottom-right corner: snaps to both edges */
    wuss_mouse_move(wuss, POINT(grab.x + scr.w - start.x1 - 2, grab.y + scr.h - start.y1 - 2), NULL);
    wuss_window_get_visible_bounds(win_sn, &visible);
    if (visible.x1 != scr.w || visible.y1 != scr.h) goto Failure;

    wuss_mouse_click(wuss, grab, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    wuss_window_close(win_sn);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_placement_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_placement_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
