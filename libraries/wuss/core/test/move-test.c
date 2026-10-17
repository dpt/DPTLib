/* core/test/move-test.c -- wuss window move blit and occlusion tests */

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

result_t wuss_move_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  wuss_window_t *hit;
  box_t          visible;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: moving/resizing a window entirely behind an occluder has no visible effect\n");

  {
    static test_task_t tc_h, tc_g;
    wuss_task_t       *delegate_h, *delegate_g;
    box_t              box_h, box_g;
    wuss_window_t     *win_h, *win_g;
    int                before_h, before_g;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 30; box_h.y1 = 30;
    rc = wuss_window_create(delegate_h,
                            &box_h,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_h),
                            SIZE2D(0, 0),
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g = mk_task(wuss, test_handle, &tc_g);
    if (delegate_g == NULL) goto Failure;

    box_g.x0 = 0;   box_g.y0 = 0;
    box_g.x1 = 150; box_g.y1 = 150; /* G is created after H, so G is topmost and fully covers H */
    rc = wuss_window_create(delegate_g,
                            &box_g,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_g),
                            SIZE2D(0, 0),
                            &win_g);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidations before measuring */
    if (rc != result_OK)
      goto Failure;

    before_h = tc_h.redraw_count;
    before_g = tc_g.redraw_count;

    wuss_window_move(win_h, POINT(60, 60)); /* still entirely within G's footprint */
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_h.redraw_count != before_h || tc_g.redraw_count != before_g)
      goto Failure; /* nothing visible changed: no redraw of either window */

    rc = wuss_window_resize(win_h, SIZE2D(25, 25)); /* still entirely within G's footprint */
    if (rc != result_OK)
      goto Failure;
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_h.redraw_count != before_h || tc_g.redraw_count != before_g)
      goto Failure;

    wuss_window_close(win_h);
    wuss_window_close(win_g);
  }

  printf("test: bring-to-front only invalidates the newly-uncovered part\n");

  {
    static test_task_t tc_i, tc_j;
    wuss_task_t       *delegate_i, *delegate_j;
    box_t              box_i, box_j, dirty;
    wuss_window_t     *win_i, *win_j;

    tc_i.redraw_count = 0;
    tc_i.mouse_count  = 0;
    delegate_i = mk_task(wuss, test_handle, &tc_i);
    if (delegate_i == NULL) goto Failure;

    box_i.x0 = 0; box_i.y0 = 0;
    box_i.x1 = 100; box_i.y1 = 100;
    rc = wuss_window_create(delegate_i,
                            &box_i,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_i),
                            SIZE2D(0, 0),
                            &win_i);
    if (rc != result_OK)
      goto Failure;

    tc_j.redraw_count = 0;
    tc_j.mouse_count  = 0;
    delegate_j = mk_task(wuss, test_handle, &tc_j);
    if (delegate_j == NULL) goto Failure;

    box_j.x0 = 50; box_j.y0 = 0;
    box_j.x1 = 150; box_j.y1 = 100; /* J created after I, so J is topmost, covering I's right half */
    rc = wuss_window_create(delegate_j,
                            &box_j,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_j),
                            SIZE2D(0, 0),
                            &win_j);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidations before measuring */
    if (rc != result_OK)
      goto Failure;

    wuss_window_restack(win_i, wuss_ZORDER_FRONT);

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 != 50 || dirty.y0 != 0 || dirty.x1 != 100 || dirty.y1 != 100)
      goto Failure; /* only I's previously-hidden right half, not its whole 0..100 footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_i);
    wuss_window_close(win_j);
  }

  printf("test: dragging off-screen and back on repaints the reappearing edge\n");

  {
    static test_task_t tc_m;
    wuss_task_t       *delegate_m;
    box_t              box_m;
    wuss_window_t     *win_m;
    int                before_m;

    tc_m.redraw_count = 0;
    tc_m.mouse_count  = 0;
    delegate_m = mk_task(wuss, test_handle, &tc_m);
    if (delegate_m == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 60; box_m.y1 = 60; /* 50x50, fully on-screen, topmost (created last) */
    rc = wuss_window_create(delegate_m,
                            &box_m,
                            NULL,
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_m),
                            SIZE2D(0, 0),
                            &win_m);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush creation invalidation, paint M's initial content */
    if (rc != result_OK)
      goto Failure;

    wuss_window_move(win_m, POINT(-40, 10)); /* slide left until half of M is off the left edge */
    rc = wuss_redraw_dirty(wuss); /* flush the vacated-sliver repaint from this move */
    if (rc != result_OK)
      goto Failure;

    before_m = tc_m.redraw_count;

    wuss_window_move(win_m, POINT(10, 10)); /* slide back: the part that re-enters the screen was
                                       * never blitted (its source pixels were off-screen),
                                       * so it must be a real task redraw, not a blit */
    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_m.redraw_count != before_m + 1)
      goto Failure; /* M must get a genuine redraw call to repaint the reappeared part */

    wuss_window_close(win_m);
  }

  printf("test: dragging a back-most window with nothing above it still blits\n");

  {
    static test_task_t tc_k, tc_l;
    wuss_task_t       *delegate_k, *delegate_l;
    box_t              box_k, box_l;
    wuss_window_t     *win_k, *win_l;
    int                before_k, before_l;

    tc_k.redraw_count = 0;
    tc_k.mouse_count  = 0;
    delegate_k = mk_task(wuss, test_handle, &tc_k);
    if (delegate_k == NULL) goto Failure;

    box_k.x0 = 0; box_k.y0 = 140; /* clear of the still-open A/B windows above */
    box_k.x1 = 50; box_k.y1 = 175;
    rc = wuss_window_create(delegate_k,
                            &box_k,
                            "K",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_k),
                            SIZE2D(0, 0),
                            &win_k);
    if (rc != result_OK)
      goto Failure;

    tc_l.redraw_count = 0;
    tc_l.mouse_count  = 0;
    delegate_l = mk_task(wuss, test_handle, &tc_l);
    if (delegate_l == NULL) goto Failure;

    box_l.x0 = 120; box_l.y0 = 140; /* well clear of K, so never overlaps it */
    box_l.x1 = 170; box_l.y1 = 175;
    rc = wuss_window_create(delegate_l,
                            &box_l,
                            "L",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_l),
                            SIZE2D(0, 0),
                            &win_l);
    if (rc != result_OK)
      goto Failure;

    wuss_window_restack(win_k, wuss_ZORDER_BACK); /* K is no longer topmost, but L never overlaps it */

    rc = wuss_redraw_dirty(wuss); /* flush the restack's own dirty region first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_k, &visible);

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 31, visible.y0 + 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* K's titlebar, clear of the close icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_k)
      goto Failure;

    before_k = tc_k.redraw_count;
    before_l = tc_l.redraw_count;
    rc = wuss_mouse_move(wuss, POINT(visible.x0 + 45, visible.y0 + 21), &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_k)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_k.redraw_count != before_k || tc_l.redraw_count != before_l)
      goto Failure; /* blitted, not redrawn: nothing above K overlapped its old
                      * footprint, so the move fast path must still apply even
                      * though K isn't topmost */

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 45, visible.y0 + 21), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_k);
    wuss_window_close(win_l);
  }

  printf("test: dragging a clear window onto an occluder leaves the occluder untouched\n");

  {
    static test_task_t tc_n, tc_o;
    wuss_task_t       *delegate_n, *delegate_o;
    box_t              box_n, box_o, visible_o, exposed, occluded, region;
    wuss_window_t     *win_n, *win_o;
    int                i, exposed_dirty, occluded_dirty;

    tc_n.redraw_count = 0;
    tc_n.mouse_count  = 0;
    delegate_n = mk_task(wuss, test_handle, &tc_n);
    if (delegate_n == NULL) goto Failure;

    box_n.x0 = 0; box_n.y0 = 140; /* clear of any occluder to start */
    box_n.x1 = 60; box_n.y1 = 170;
    rc = wuss_window_create(delegate_n, &box_n, "N",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_n),
                            SIZE2D(0, 0),
                            &win_n);
    if (rc != result_OK)
      goto Failure;

    tc_o.redraw_count = 0;
    tc_o.mouse_count  = 0;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_o.x0 = 90; box_o.y0 = 140; /* N will be dragged partly on top of O */
    box_o.x1 = 130; box_o.y1 = 180;
    rc = wuss_window_create(delegate_o, &box_o, "O",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            box_size(&box_o),
                            SIZE2D(0, 0),
                            &win_o);
    if (rc != result_OK)
      goto Failure;

    /* O is created after N, so O is topmost -- N's destination footprint
     * will overlap an occluder above it in z-order. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_n, &visible);

    /* Move N far enough right that its new footprint lands partly under O
     * (which stays wholly untouched), while its old footprint started
     * entirely clear of O. */
    wuss_window_move(win_n, POINT(visible.x0 + 80, visible.y0 + 10));

    wuss_window_get_visible_bounds(win_n, &visible);
    wuss_window_get_visible_bounds(win_o, &visible_o);
    exposed.x0 = visible.x0;   exposed.y0 = visible.y0;
    exposed.x1 = visible_o.x0; exposed.y1 = visible.y1; /* N's part left of O */
    box_intersection(&visible, &visible_o, &occluded); /* N's part under O */

    /* The part of N's new footprint that lands under O must NOT be queued
     * dirty -- O hasn't moved, so its pixels are already correct there, and
     * the move blit must have skipped blitting into that area rather than
     * pasting N's stale pixels over it and forcing a repair. */
    occluded_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &occluded))
        occluded_dirty = 1;
    }
    if (occluded_dirty)
      goto Failure;

    /* The exposed part of N's new footprint, not under any occluder, must
     * NOT be queued dirty -- it was already moved there correctly by the
     * blit, so redrawing it too would be exactly the "repaint what could
     * have been left in place" waste this fast path exists to avoid. */
    exposed_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &exposed))
        exposed_dirty = 1;
    }
    if (exposed_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_n);
    wuss_window_close(win_o);
  }

  printf("test: moving a partly-occluded window blits its clean part and only repaints the occluded part\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_a, visible_b_before;
    box_t              clean_new, hidden_new, region;
    wuss_window_t     *win_a, *win_b;
    int                i, dx, clean_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 20; box_b.y0 = 10; /* left half will sit under A */
    box_b.x1 = 80; box_b.y1 = 50;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 40; box_a.y1 = 100;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (x:20-80,y:10-50) is split by A (x:0-40) into a
     * hidden strip (x:20-40, under A) and a clean strip (x:40-80, exposed). */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    /* Move B far enough right that its whole new footprint clears A. */
    dx = 60;
    wuss_window_move(win_b, POINT(visible_b_before.x0 + dx,
                                        visible_b_before.y0));

    /* The clean strip (previously exposed, genuinely B's own pixels) lands
     * at its translated destination and must have been blitted there, not
     * repainted. */
    clean_new.x0 = 40 + dx; clean_new.y0 = 10;
    clean_new.x1 = 80 + dx; clean_new.y1 = 50;

    /* The hidden strip (previously under A, never B's valid rendering) has
     * no valid source pixels, so its translated destination must be a real
     * repaint. */
    hidden_new.x0 = 20 + dx; hidden_new.y0 = 10;
    hidden_new.x1 = 40 + dx; hidden_new.y1 = 50;

    hidden_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &hidden_new))
        hidden_dirty = 1;
    }
    if (!hidden_dirty)
      goto Failure;

    clean_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &clean_new))
        clean_dirty = 1;
    }
    if (clean_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: moving a window whose occluded piece was never blitted doesn't redraw the occluder\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_a, visible_b_before;
    box_t              region;
    wuss_window_t     *win_a, *win_b;
    int                i, occluder_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 0; box_b.y0 = 0; /* right part sits under A throughout */
    box_b.x1 = 60; box_b.y1 = 40;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 40; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 40;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    /* Move B straight down: its clean piece (x:0-40) and hidden piece
     * (x:40-60, under A) both stay clear of / under A exactly as before --
     * nothing about A's own pixels is ever touched by the blit, so A must
     * not be forced to redraw. */
    wuss_window_move(win_b, POINT(visible_b_before.x0,
                                        visible_b_before.y0 + 5));

    occluder_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &visible_a))
        occluder_dirty = 1;
    }
    if (occluder_dirty)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: moving a window split by a mid-band occluder past the gap between bands blits both bands in a safe order\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_b_before;
    box_t              occluded_overlap, hidden_new, region;
    wuss_window_t     *win_a, *win_b;
    int                i, occluded_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 0; box_b.y0 = 0; /* middle band sits under A */
    box_b.x1 = 60; box_b.y1 = 60;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 0; box_a.y0 = 20; /* created after B, so A is topmost */
    box_a.x1 = 60; box_a.y1 = 40;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (y:0-60) is split by A (y:20-40) into a top clean
     * band (y:0-20), a hidden middle band (y:20-40) and a bottom clean band
     * (y:40-60), each spanning the full width. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);

    /* Move B down by 25px, past the 20px gap between the two clean bands:
     * the top band's destination (y:25-45) lands on the bottom band's
     * still-unread old source (y:40-60). Blitted in the other order --
     * bottom band first (its destination y:65-85 doesn't touch the top
     * band's source), then the top band -- both blits are safe, so this
     * is not a genuine clobber cycle (translating disjoint pieces by the
     * same offset never produces one: any conflict is consistently
     * oriented by the direction of the move). The top band's destination
     * overlap with A (y:25-40) is skipped by the blit entirely (A hasn't
     * moved, its pixels there are already correct), so it must stay clean;
     * the translated hidden band (y:45-65, never had valid pixels) still
     * needs forcing dirty for a real repaint. */
    wuss_window_move(win_b, POINT(visible_b_before.x0,
                                        visible_b_before.y0 + 25));

    occluded_overlap.x0 = 0;  occluded_overlap.y0 = 25;
    occluded_overlap.x1 = 60; occluded_overlap.y1 = 40;
    hidden_new.x0       = 0;  hidden_new.y0       = 45;
    hidden_new.x1       = 60; hidden_new.y1       = 65;

    occluded_dirty = hidden_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_intersects(&region, &occluded_overlap))
        occluded_dirty = 1;
      if (box_contains_box(&hidden_new, &region))
        hidden_dirty = 1;
    }
    if (occluded_dirty || !hidden_dirty)
      goto Failure;

    /* The blit must have actually happened, not fallen back: the part of
     * B's new footprint that's clear of A and not the hidden band (e.g.
     * the bottom band's new position, y:65-85) must not be dirtied. */
    {
      box_t clean_after, dirty_check;

      clean_after.x0 = 0;  clean_after.y0 = 65;
      clean_after.x1 = 60; clean_after.y1 = 85;
      for (i = 0; i < wuss_get_dirty_count(wuss); i++)
      {
        wuss_get_dirty(wuss, i, &region);
        if (!box_intersection(&region, &clean_after, &dirty_check))
          goto Failure;
      }
    }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  printf("test: dragging a window deeper under a corner occluder blits both L-shaped pieces in a safe order\n");

  {
    static test_task_t tc_a, tc_b;
    wuss_task_t       *delegate_a, *delegate_b;
    box_t              box_a, box_b, visible_b_before, visible_a, region;
    wuss_window_t     *win_a, *win_b;
    int                i;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b = mk_task(wuss, test_handle, &tc_b);
    if (delegate_b == NULL) goto Failure;

    box_b.x0 = 80; box_b.y0 = 80; /* corner already under A */
    box_b.x1 = 140; box_b.y1 = 140;
    rc = wuss_window_create(delegate_b, &box_b, "B",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a = mk_task(wuss, test_handle, &tc_a);
    if (delegate_a == NULL) goto Failure;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 100;
    rc = wuss_window_create(delegate_a, &box_a, "A",
                            /* fully chromeless: no titlebar, outline, scrollbars or resize */
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                            wuss_NO_BACKDROP,
                            box_size(&box_a),
                            SIZE2D(0, 0),
                            &win_a);
    if (rc != result_OK)
      goto Failure;

    /* B's old footprint (80,80)-(140,140) overlaps A (0,0)-(100,100) in its
     * corner (80,80)-(100,100); the rest of B is split into an L-shaped
     * clean region of two pieces, one of whose destination lands on the
     * other's still-unread source -- but blitting the other piece first
     * avoids that entirely, so this must NOT fall back to a full clipped
     * redraw (that was the "Adjust drag behind a corner fully redraws the
     * window" regression). Dragging B up-left by (-15,-15) also grows the
     * overlap with A without ever fully hiding or fully clearing it -- the
     * blit skips the part of each piece's destination that now lands under
     * A, so A's own rendering there is never touched and needs no repair. */

    rc = wuss_redraw_dirty(wuss); /* flush both creations first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_b, &visible_b_before);
    wuss_window_get_visible_bounds(win_a, &visible_a);

    wuss_window_move(win_b, POINT(visible_b_before.x0 - 15,
                                        visible_b_before.y0 - 15));

    /* The blit must have actually happened, not fallen back: B's own
     * footprint (outside A) must not be dirtied wholesale. */
    {
      box_t visible_b_after, whole_footprint, dirty_area_box;
      int   dirty_area, footprint_area;

      wuss_window_get_visible_bounds(win_b, &visible_b_after);
      box_union(&visible_b_before, &visible_b_after, &whole_footprint);

      dirty_area = 0;
      for (i = 0; i < wuss_get_dirty_count(wuss); i++)
      {
        wuss_get_dirty(wuss, i, &region);
        if (!box_intersection(&region, &whole_footprint, &dirty_area_box))
          dirty_area += (dirty_area_box.x1 - dirty_area_box.x0) *
                        (dirty_area_box.y1 - dirty_area_box.y0);
      }
      footprint_area = (whole_footprint.x1 - whole_footprint.x0) *
                       (whole_footprint.y1 - whole_footprint.y0);
      if (dirty_area >= footprint_area)
        goto Failure; /* fell back to a full redraw instead of blitting */
    }

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_b);
    wuss_window_close(win_a);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_move_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_move_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
