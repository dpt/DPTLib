/* core/test/toolbar-test.c -- wuss toolbar window tests */

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

#define TB_FLAGS (wuss_WINDOW_NO_TITLEBAR)

/* Non-zero if want[0..n-1] are adjacent, front to back, in the z-order. */
static int z_run_is(wuss_t *wuss, wuss_window_t *const *want, int n)
{
  wuss_window_t *w;
  int            i;

  for (w = wuss__z_first(wuss); w != NULL && w != want[0];
       w = wuss__z_below(w))
    ;

  for (i = 0; i < n; i++, w = wuss__z_below(w))
    if (w != want[i])
      return 0;

  return 1;
}

static result_t mk_toolbar(wuss_task_t    *task,
                           int             w,
                           int             h,
                           wuss_window_t **window)
{
  box_t box;

  box = (box_t) BOX_POS_SIZE(150, 150, w, h);

  return wuss_window_create(task, &box, NULL, TB_FLAGS, wuss_NO_BACKDROP,
                            SIZE2D(w, h), SIZE2D(0, 0), window);
}

result_t wuss_toolbar_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 300, 300);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: toolbars: placement, stacking, screen clamp, z-order, hiding, focus and closing\n");

  {
    wuss_task_t   *owner, *other;
    wuss_window_t *parent, *left1, *left2, *right, *top, *stray, *bad;
    wuss_window_t *order[4];
    box_t          box, pc, pv, v1, v2, vr, vt, c1, c2, cr, ct;

    owner = mk_task(wuss, paint_handle, NULL);
    other = mk_task(wuss, paint_handle, NULL);
    if (owner == NULL || other == NULL) goto Failure;

    box = (box_t) BOX_POS_SIZE(100, 100, 80, 60);
    rc  = wuss_window_create(owner, &box, "P", wuss_WINDOW_DEFAULT |
                             wuss_WINDOW_FOCUSABLE, wuss_NO_BACKDROP,
                             SIZE2D(200, 200), SIZE2D(0, 0), &parent);
    if (rc != result_OK) goto Failure;

    if (mk_toolbar(owner, 20, 30, &left1) != result_OK) goto Failure;
    if (mk_toolbar(owner, 10, 30, &left2) != result_OK) goto Failure;
    if (mk_toolbar(owner, 16, 20, &right) != result_OK) goto Failure;
    if (mk_toolbar(owner, 40, 12, &top)   != result_OK) goto Failure;

    /* refused: titlebar furniture, other task, self, nesting */
    box = (box_t) BOX_POS_SIZE(10, 10, 20, 20);
    rc  = wuss_window_create(owner, &box, "B", wuss_WINDOW_DEFAULT,
                             wuss_NO_BACKDROP, SIZE2D(20, 20), SIZE2D(0, 0),
                             &bad);
    if (rc != result_OK) goto Failure;
    if (wuss_window_attach(bad, parent, wuss_EDGE_LEFT, wuss_ALIGN_TOP, 0) !=
        result_BAD_ARG) goto Failure;
    wuss_window_close(bad);

    if (mk_toolbar(other, 20, 20, &stray) != result_OK) goto Failure;
    if (wuss_window_attach(stray, parent, wuss_EDGE_LEFT, wuss_ALIGN_TOP, 0)
        != result_BAD_ARG) goto Failure;
    wuss_window_close(stray);

    if (wuss_window_attach(parent, parent, wuss_EDGE_LEFT, wuss_ALIGN_TOP,
                           0) != result_BAD_ARG) goto Failure;

    if (wuss_window_attach(left1, parent, wuss_EDGE_LEFT, wuss_ALIGN_TOP, 0)
        != result_OK) goto Failure;
    if (wuss_window_attach(left2, parent, wuss_EDGE_LEFT, wuss_ALIGN_TOP, 5)
        != result_OK) goto Failure;
    if (wuss_window_attach(right, parent, wuss_EDGE_RIGHT,
                           wuss_ALIGN_BOTTOM, 0) != result_OK) goto Failure;
    if (wuss_window_attach(top, parent, wuss_EDGE_TOP, wuss_ALIGN_CENTRE, 0)
        != result_OK) goto Failure;

    /* nesting either way round */
    if (wuss_window_attach(top, left1, wuss_EDGE_LEFT, wuss_ALIGN_TOP, 0) !=
        result_BAD_ARG) goto Failure;
    if (wuss_window_attach(parent, left1, wuss_EDGE_LEFT, wuss_ALIGN_TOP, 0)
        != result_BAD_ARG) goto Failure;

    /* outside the footprint, outlines overlapping, content aligned against
     * the content area, the second left toolbar stacked outside the first */
    wuss_window_get_visible_bounds(parent, &pv);
    wuss_window_get_content_bounds(parent, &pc);
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_get_visible_bounds(left2, &v2);
    wuss_window_get_visible_bounds(right, &vr);
    wuss_window_get_visible_bounds(top, &vt);
    wuss_window_get_content_bounds(left1, &c1);
    wuss_window_get_content_bounds(left2, &c2);
    wuss_window_get_content_bounds(right, &cr);
    wuss_window_get_content_bounds(top, &ct);
    if (v1.x1 != pv.x0 + 1 || c1.y0 != pc.y0) goto Failure;
    if (v2.x1 != v1.x0 + 1 || c2.y0 != pc.y0 + 5) goto Failure;
    if (vr.x0 != pv.x1 - 1 || cr.y1 != pc.y1) goto Failure;
    if (vt.y1 != pv.y0 + 1 ||
        ct.x0 != (pc.x0 + pc.x1 - (ct.x1 - ct.x0)) / 2) goto Failure;

    /* a toolbar can't be moved by itself */
    wuss_window_move(left1, POINT(5, 5));
    wuss_window_get_visible_bounds(left1, &v1);
    if (v1.x1 != pv.x0 + 1) goto Failure;

    /* the group moves together */
    wuss_window_move(parent, POINT(pc.x0 + 10, pc.y0 + 20));
    wuss_window_get_visible_bounds(parent, &pv);
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_get_visible_bounds(top, &vt);
    if (v1.x1 != pv.x0 + 1 || vt.y1 != pv.y0 + 1) goto Failure;

    /* near the left screen edge the left stack clamps as a block,
     * overlapping the parent... */
    wuss_window_get_content_bounds(parent, &pc);
    wuss_window_move(parent, POINT(pc.x0 - pv.x0 + 10, pc.y0));
    wuss_window_get_visible_bounds(parent, &pv);
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_get_visible_bounds(left2, &v2);
    if (pv.x0 != 10 || v2.x0 != 0 || v2.x1 != v1.x0 + 1) goto Failure;

    /* ...until the parent's edge itself leaves the screen */
    wuss_window_get_content_bounds(parent, &pc);
    wuss_window_move(parent, POINT(pc.x0 - 15, pc.y0));
    wuss_window_get_visible_bounds(parent, &pv);
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_get_visible_bounds(left2, &v2);
    if (pv.x0 != -5 || v2.x0 != -5 || v2.x1 != v1.x0 + 1) goto Failure;

    /* near the right edge: clamped, overlapping the parent */
    wuss_window_get_content_bounds(parent, &pc);
    wuss_window_move(parent, POINT(pc.x0 + 295 - pv.x1, pc.y0));
    wuss_window_get_visible_bounds(parent, &pv);
    wuss_window_get_visible_bounds(right, &vr);
    if (pv.x1 != 295 || vr.x1 != 300) goto Failure;

    /* resizing the parent moves the toolbars but keeps their size */
    wuss_window_move(parent, POINT(100, 100));
    rc = wuss_window_resize(parent, SIZE2D(60, 50));
    if (rc != result_OK) goto Failure;
    wuss_window_get_visible_bounds(parent, &pv);
    wuss_window_get_content_bounds(parent, &pc);
    wuss_window_get_visible_bounds(right, &vr);
    wuss_window_get_content_bounds(right, &cr);
    if (vr.x0 != pv.x1 - 1 || cr.y1 != pc.y1 || vr.x1 - vr.x0 != 16 + 2)
      goto Failure;

    /* resizing a toolbar re-lays out its siblings */
    rc = wuss_window_resize(left1, SIZE2D(30, 30));
    if (rc != result_OK) goto Failure;
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_get_visible_bounds(left2, &v2);
    if (v1.x1 != pv.x0 + 1 || v2.x1 != v1.x0 + 1) goto Failure;

    /* the group restacks together, toolbars directly above the parent */
    box = (box_t) BOX_POS_SIZE(20, 20, 50, 50);
    rc  = wuss_window_create(owner, &box, "S", wuss_WINDOW_DEFAULT,
                             wuss_NO_BACKDROP, SIZE2D(50, 50), SIZE2D(0, 0),
                             &stray);
    if (rc != result_OK) goto Failure;

    wuss_window_restack(left1, wuss_ZORDER_FRONT);
    order[0] = top; order[1] = right; order[2] = left2; order[3] = left1;
    if (!z_run_is(wuss, order, 4)) goto Failure;
    order[0] = left1; order[1] = parent;
    if (!z_run_is(wuss, order, 2)) goto Failure;
    if (wuss__z_first(wuss) != top) goto Failure;

    wuss_window_restack(parent, wuss_ZORDER_BACK);
    order[0] = stray; order[1] = left1;
    if (!z_run_is(wuss, order, 2)) goto Failure;
    order[0] = top; order[1] = parent;
    if (!z_run_is(wuss, order, 2)) goto Failure;
    wuss_window_close(stray);

    /* a hidden toolbar frees its slot */
    rc = wuss_window_set_hidden(left1, 1);
    if (rc != result_OK) goto Failure;
    wuss_window_get_visible_bounds(left2, &v2);
    if (v2.x1 != pv.x0 + 1) goto Failure;
    rc = wuss_window_set_hidden(left1, 0);
    if (rc != result_OK) goto Failure;
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_get_visible_bounds(left2, &v2);
    if (v1.x1 != pv.x0 + 1 || v2.x1 != v1.x0 + 1) goto Failure;

    /* hiding the parent hides its toolbars; showing brings back only the
     * ones it hid */
    rc = wuss_window_set_hidden(right, 1);
    if (rc != result_OK) goto Failure;
    rc = wuss_window_set_hidden(parent, 1);
    if (rc != result_OK) goto Failure;
    if (!(left1->flags & wuss_WINDOW_HIDDEN) ||
        !(top->flags & wuss_WINDOW_HIDDEN)) goto Failure;
    rc = wuss_window_set_hidden(parent, 0);
    if (rc != result_OK) goto Failure;
    if ((left1->flags & wuss_WINDOW_HIDDEN) ||
        (top->flags & wuss_WINDOW_HIDDEN)   ||
        !(right->flags & wuss_WINDOW_HIDDEN)) goto Failure;
    rc = wuss_window_set_hidden(right, 0);
    if (rc != result_OK) goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;

    /* a press on a toolbar that can't take the focus gives it to the
     * parent */
    wuss_window_get_content_bounds(left1, &v1);
    wuss_mouse_click(wuss, POINT(v1.x0 + 2, v1.y0 + 2), wuss_BUTTON_SELECT,
                     wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(v1.x0 + 2, v1.y0 + 2), wuss_BUTTON_SELECT,
                     wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != parent) goto Failure;

    /* a detached toolbar stays put and its siblings close up */
    wuss_window_get_visible_bounds(left1, &v1);
    wuss_window_detach(left1);
    wuss_window_get_visible_bounds(left1, &box);
    wuss_window_get_visible_bounds(left2, &v2);
    if (box.x0 != v1.x0 || v2.x1 != pv.x0 + 1) goto Failure;
    wuss_window_close(left1);

    /* closing a toolbar alone leaves the parent's list intact */
    wuss_window_close(right);
    if (parent->toolbar.first != left2 || left2->toolbar.next != top)
      goto Failure;

    /* closing the parent takes the rest with it */
    wuss_window_close(parent);
    if (owner->windows.next != NULL) goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_toolbar_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_toolbar_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
