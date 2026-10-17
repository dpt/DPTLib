/* core/test/toggle-test.c -- wuss toggle-size repaint and blit tests */

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

result_t wuss_toggle_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  box_t          content;
  int            width, height;
  wuss_window_t *hit;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: toggle-size blits rather than redrawing the whole window\n");

  {
    static test_task_t tc_t;
    wuss_task_t       *delegate_t;
    box_t              box_t_win, before, after, titlebar, toggle;
    wuss_window_t     *win_t;
    int                outline_px, titlebar_height, inset, icon;
    int                i, dirty_area, full_area, cx, cy, old_icon_x, old_icon_y, found;
    int                interior_x, interior_y, interior_dirty;
    int                old_vscroll_x, old_vscroll_y, old_vscroll_found;

    tc_t.redraw_count = 0;
    tc_t.mouse_count  = 0;
    delegate_t = mk_task(wuss, test_handle, &tc_t);
    if (delegate_t == NULL) goto Failure;

    box_t_win.x0 = 10; box_t_win.y0 = 10;
    box_t_win.x1 = 50; box_t_win.y1 = 50; /* 40x40 content, room to grow to a 150x150 doc without the toggled box needing to be repositioned off (10,10) -- this test is about the in-place grow blit */
    rc = wuss_window_create(delegate_t, &box_t_win, "T", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(150, 150), SIZE2D(0, 0), &win_t);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    /* toggle icon: top-right of the titlebar, inset by 3px, sized 20 - 2*3
     * (default titlebar height 20, WUSS_BUTTON_INSET 3), matching
     * wuss__toggle_box's formula -- mirrored here since the test only sees
     * the public API */
    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_t, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;
    old_icon_x = cx; old_icon_y = cy; /* pre-grow icon centre: ends up mid-titlebar once the window widens */

    /* pre-grow content interior, well clear of outline/titlebar/scrollbar
     * furniture on every side (carve.x == carve.y == icon here, since both
     * scrollbars are enabled) -- a point the blit must genuinely have
     * preserved, unlike a naive summed-region-area comparison, which
     * overcounts once furniture invalidation adds several regions that
     * overlap each other and the grown-edge region without merging (only
     * exact-edge-aligned boxes merge; see box_merge in geom/box/list-add.c) */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    /* pre-grow vscroll column, at the *old* right edge: once the window
     * widens this sits mid-content rather than at the (now further right)
     * new column, inside the region the blit reuses as valid pixels --
     * the content redraw never touches it, so only a forced old-furniture
     * invalidate stops the old scrollbar glyph being left behind there */
    old_vscroll_x = before.x1 - outline_px - icon / 2;
    old_vscroll_y = interior_y;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* T's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_t)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    found             = 0;
    interior_dirty    = 0;
    old_vscroll_found = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, old_icon_x, old_icon_y))
        found = 1;
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
      if (box_contains_point(&region, old_vscroll_x, old_vscroll_y))
        old_vscroll_found = 1;
    }
    if (!old_vscroll_found)
      goto Failure; /* the old vscroll column, now mid-content rather than at
                      * the (further right) new right edge, falls inside both
                      * "before" and the grown "visible" same as the toggle
                      * icon above -- the blit alone leaves its stale pixels
                      * on screen unless the old furniture position is also
                      * forced dirty */
    if (!found)
      goto Failure; /* the old toggle-icon glyph, now mid-titlebar rather than
                      * at its corner, falls inside both "before" and the
                      * grown "visible" -- the content blit alone would leave
                      * it un-redrawn as a ghost; furniture must be forced
                      * dirty separately since its layout depends on size */

    if (interior_dirty)
      goto Failure; /* interior content pixel, untouched by any furniture or
                      * grown-edge region: the blit must have reused it */

    wuss_window_get_visible_bounds(win_t, &after);
    if (after.x1 > 200 || after.y1 > 200)
      goto Failure; /* toggling must leave the window fully on-screen */
    if (after.x0 != before.x0 || after.y0 != before.y0)
      goto Failure; /* a 150x150 doc toggled from (10,10) fits without moving,
                      * so the top-left stays put and the in-place grow blit
                      * (asserted below) applies */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Icon moved with the grown titlebar, so recompute. */
    wuss_window_get_visible_bounds(win_t, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* T's toggle-size icon: shrink */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_t)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    /* a point in the grown box's old bottom-right corner, well outside the
     * shrunk box: on the shrink half of this same toggle, this is where the
     * old titlebar/outline/scrollbar strips used to sit and don't any more
     * at all -- nothing repaints that pixel unless the vacated old
     * footprint outside the new one is invalidated too, not just the old
     * and new *content* boxes (the bug this reproduces: the strip was left
     * showing stale furniture after a shrink). */
    wuss_window_get_visible_bounds(win_t, &after);
    if (after.x1 >= before.x1 || after.y1 >= before.y1)
      goto Failure; /* sanity: this toggle must actually be a shrink */

    old_vscroll_found = 0; /* reused as "vacated old corner found" below */
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, before.x1 - 1, before.y1 - 1))
        old_vscroll_found = 1;
    }
    if (!old_vscroll_found)
      goto Failure; /* the vacated old furniture corner was left stale */

    dirty_area = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }

    full_area = (before.x1 - before.x0) * (before.y1 - before.y0); /* the grown box: what a full-union invalidate would have covered */
    if (dirty_area >= full_area)
      goto Failure; /* vacated edge only, not the whole grown footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_t);
  }

  printf("test: toggle-size still blits when a hidden window sits above it in z-order\n");

  {
    static test_task_t tc_h;
    wuss_task_t       *delegate_h;
    box_t              box_h_win, hidden_box, before, toggle, titlebar;
    wuss_window_t     *win_h, *win_hidden;
    int                outline_px, titlebar_height, inset, icon, cx, cy;
    int                i, interior_x, interior_y, interior_dirty;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h_win.x0 = 10; box_h_win.y0 = 10;
    box_h_win.x1 = 50; box_h_win.y1 = 50; /* same in-place grow shape as the
                                            * plain toggle-blit test above */
    rc = wuss_window_create(delegate_h, &box_h_win, "H", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(150, 150), SIZE2D(0, 0), &win_h);
    if (rc != result_OK)
      goto Failure;

    /* a hidden window created afterwards still lands at the z-order head
     * (list_add_to_head in window/create.c, unconditional on
     * wuss_WINDOW_HIDDEN) -- it must not be mistaken for an occluder sitting
     * above win_h, or wuss__furniture_toggle_size's topmost check wrongly
     * declines the blit and falls back to a full old+new footprint repaint */
    hidden_box.x0 = 60; hidden_box.y0 = 60;
    hidden_box.x1 = 100; hidden_box.y1 = 100;
    rc = wuss_window_create(delegate_h, &hidden_box, "Hidden",
                            wuss_WINDOW_HIDDEN, wuss_NO_BACKDROP,
                            SIZE2D(40, 40), SIZE2D(0, 0), &win_hidden);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates' own invalidates */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_h, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* pre-grow content interior, well clear of outline/titlebar/scrollbar
     * furniture on every side -- same reasoning as the plain toggle-blit
     * test above: a pixel the blit must have reused rather than repainted */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* H's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_h)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (interior_dirty)
      goto Failure; /* a hidden occluder above in z-order must not defeat the
                      * blit fast path -- if it did, this interior content
                      * pixel (which the blit alone would have reused
                      * untouched) would be swept into the no-blit
                      * fallback's full old+new footprint repaint instead */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_hidden);
    wuss_window_close(win_h);
  }

  printf("test: toggle-size that forces a scroll re-clamp invalidates the content it's about to redraw at the new offset, not just the blit's edge sliver\n");

  {
    static test_task_t tc_r;
    wuss_task_t       *delegate_r;
    box_t              box_r, before, titlebar, toggle;
    wuss_window_t     *win_r;
    int                outline_px, titlebar_height, inset, icon;
    int                i, interior_x, interior_y, interior_dirty, cx, cy;

    tc_r.redraw_count = 0;
    tc_r.mouse_count  = 0;
    delegate_r = mk_task(wuss, test_handle, &tc_r);
    if (delegate_r == NULL) goto Failure;

    box_r.x0 = 10; box_r.y0 = 10;
    box_r.x1 = 50; box_r.y1 = 50; /* 40x40 content; doc bigger than that, so it starts scrollable */
    rc = wuss_window_create(delegate_r, &box_r, "R", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(70, 70), SIZE2D(0, 0), &win_r);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_r, POINT(0, 15)); /* within range: max_y = 70 - 40 = 30 */

    rc = wuss_redraw_dirty(wuss); /* flush the scroll's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_r, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* interior of the OLD content box, clear of outline/titlebar/scrollbar
     * furniture -- well inside "before", so a plain grow (no re-clamp) would
     * leave it untouched by the blit-reuse optimisation, same as the
     * "toggle-size blits" test above. But this window starts scrolled, and
     * growing to doc_height (70) here forces content_size up to doc_size,
     * clamping scroll.y back to 0 -- the content this point shows is stale
     * regardless of the blit, so it must be invalidated outright. */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* R's toggle-size icon: grow past doc_height, forcing a scroll re-clamp */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_r)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (!interior_dirty)
      goto Failure; /* the re-clamp changed scroll.y, so every pixel in the
                      * content box is now showing the wrong offset -- if
                      * this interior point (untouched by the toggle's own
                      * blit-reuse/furniture invalidation) isn't marked
                      * dirty, the fix has regressed to relying on
                      * wuss_window_set_scroll's live blit-and-shift, which
                      * is invalid mid-toggle: the screen doesn't reflect
                      * the new geometry yet at that point */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_r);
  }

  printf("test: toggle-size's forced full repaint after a scroll re-clamp paints every content pixel at the new offset, none stale from the old\n");

  {
    /* Companion to the test above: that one only checks the interior point
     * got marked dirty. This checks the actual pixels after the redraw are
     * correct at the post-reclamp scroll offset (0), not left over from the
     * pre-toggle offset (15) or corrupted by toggle-action.c's window-move
     * blit running over a region the scroll invalidate also claimed.
     * paint_handle paints one-pixel rows keyed by doc_y in the blue channel. */
    colour_t           cg;
    static test_task_t tc_g;
    wuss_task_t       *delegate_g;
    box_t              box_g, before_g, content_g, titlebar_g, toggle_g;
    wuss_window_t     *win_g;
    int                outline_px, titlebar_height, inset, icon, cx, cy;
    int                gx, gy, bad;

    cg = colour_rgb(0x33, 0x44, 0x55);
    tc_g.redraw_count = 0; tc_g.mouse_count = 0;
    delegate_g = mk_task(wuss, paint_handle, &cg);
    if (delegate_g == NULL) goto Failure;

    box_g.x0 = 110; box_g.y0 = 10;
    box_g.x1 = 150; box_g.y1 = 50; /* 40x40 content; doc bigger, starts scrollable */
    rc = wuss_window_create(delegate_g, &box_g, "G", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(70, 70), SIZE2D(0, 0), &win_g);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_g, POINT(0, 15)); /* within range: max_y = 70 - 40 = 30 */
    rc = wuss_redraw_dirty(wuss); /* flush the scroll's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_g, &before_g);
    titlebar_g.x0 = before_g.x0 + outline_px;
    titlebar_g.x1 = before_g.x1 - outline_px;
    titlebar_g.y0 = before_g.y0 + outline_px;
    toggle_g.x1 = titlebar_g.x1 - inset;
    toggle_g.x0 = toggle_g.x1 - icon;
    toggle_g.y0 = titlebar_g.y0 + inset;
    toggle_g.y1 = toggle_g.y0 + icon;
    cx = (toggle_g.x0 + toggle_g.x1) / 2;
    cy = (toggle_g.y0 + toggle_g.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's toggle-size icon: grow past doc_height, forcing a scroll re-clamp */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every row of the (now grown, scroll.y == 0) content box must show the
     * doc_y its own new offset puts there */
    wuss_window_get_content_bounds(win_g, &content_g);
    bad = 0;
    gx  = (content_g.x0 + content_g.x1) / 2;
    for (gy = content_g.y0; gy < content_g.y1; gy++)
    {
      uint32_t px, blue;

      px   = ((const uint32_t *) fixture.pixels)[gy * 200 + gx];
      blue = px & 0xff;
      if (blue != (uint32_t) ((gy - content_g.y0) & 0xff))
        bad = 1;
    }
    if (bad)
      goto Failure; /* a stale pre-toggle-offset or corrupted pixel survived
                      * the forced full repaint */

    wuss_window_close(win_g);
  }

  printf("test: toggle-size grow-in-place (no scroll re-clamp) never queues two dirty rects covering the same furniture pixels\n");

  {
    /* A grow-in-place toggle (top-left unchanged) takes the blit fast path
     * in toggle-action.c: wuss__invalidate_minus(&before_content, &copied)
     * queues the vacated content sliver, then wuss__furniture_invalidate
     * queues the reflowed furniture strips at (unchanged) window->visible.
     * Before the fix, the first call was passed &before (the whole old
     * visible box, furniture included) instead of &before_content, so the
     * old titlebar/outline/scrollbar strips got queued twice -- once from
     * that call, once from wuss__furniture_invalidate -- as two rects that
     * box_list_add's containment/shared-edge merge doesn't fold together,
     * so wuss_redraw_dirty painted the overlap twice. */
    static test_task_t tc_h;
    wuss_task_t       *delegate_h;
    box_t              box_h, before_h, titlebar_h, toggle_h;
    wuss_window_t     *win_h;
    int                outline_px, titlebar_height, inset, icon, cx, cy;
    int                i, j, overlap;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    /* doc capped at 150x150 (as in the "toggle-size blits" test above) so
     * growing has room to stay put without the screen-edge nudge moving the
     * top-left -- the blitted branch runs, no scroll re-clamp */
    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 50; box_h.y1 = 50;
    rc = wuss_window_create(delegate_h, &box_h, "H", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(150, 150), SIZE2D(0, 0), &win_h);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    /* box_h is the content box passed to create, not the actual (possibly
     * nudged-on-screen) visible box -- read it back for real coordinates */
    wuss_window_get_visible_bounds(win_h, &before_h);
    titlebar_h.x0 = before_h.x0 + outline_px;
    titlebar_h.x1 = before_h.x1 - outline_px;
    titlebar_h.y0 = before_h.y0 + outline_px;
    toggle_h.x1 = titlebar_h.x1 - inset;
    toggle_h.x0 = toggle_h.x1 - icon;
    toggle_h.y0 = titlebar_h.y0 + inset;
    toggle_h.y1 = toggle_h.y0 + icon;
    cx = (toggle_h.x0 + toggle_h.x1) / 2;
    cy = (toggle_h.y0 + toggle_h.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* H's toggle-size icon: grow in place */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_h)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    overlap = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss) && !overlap; i++)
    {
      box_t region_i;

      wuss_get_dirty(wuss, i, &region_i);
      for (j = i + 1; j < wuss_get_dirty_count(wuss) && !overlap; j++)
      {
        box_t region_j, ignored;

        wuss_get_dirty(wuss, j, &region_j);
        if (box_intersection(&region_i, &region_j, &ignored) == 0)
          overlap = 1;
      }
    }
    if (overlap)
      goto Failure; /* two dirty rects cover the same screen pixels --
                      * wuss_redraw_dirty will paint that overlap twice */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_h);
  }

  printf("test: toggle-size maximize accounts for scrollbar furniture, not just outline/titlebar\n");

  {
    static test_task_t tc_u;
    wuss_task_t       *delegate_u;
    box_t              box_u, ub, titlebar, toggle;
    wuss_window_t     *win_u;
    int                outline_px, titlebar_height, inset, icon;
    int                cx, cy;

    tc_u.redraw_count = 0;
    tc_u.mouse_count  = 0;
    delegate_u = mk_task(wuss, test_handle, &tc_u);
    if (delegate_u == NULL) goto Failure;

    box_u.x0 = 80; box_u.y0 = 80;
    box_u.x1 = 120; box_u.y1 = 120; /* 40x40 content */
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_u, "U", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(70, 70), SIZE2D(0, 0), &win_u); /* doc size well within the 200x200 screen: growth is doc-limited, not screen-limited */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_u, &ub);
    titlebar.x0 = ub.x0 + outline_px;
    titlebar.x1 = ub.x1 - outline_px;
    titlebar.y0 = ub.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* U's toggle-size icon: grow to doc size */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_u)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_u, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;
    if (width != 70 || height != 70)
      goto Failure; /* visible must grow by the scrollbar breadth too, on top
                      * of outline/titlebar, or content ends up icon-size
                      * short of doc_width/doc_height */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Icon moved with the grown titlebar, so recompute. */
    wuss_window_get_visible_bounds(win_u, &ub);
    titlebar.x0 = ub.x0 + outline_px;
    titlebar.x1 = ub.x1 - outline_px;
    titlebar.y0 = ub.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* U's toggle-size icon: shrink back */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_u)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_u, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;
    if (width != 40 || height != 40)
      goto Failure; /* restores exactly the pre-toggle content size */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_u);
  }

  printf("test: toggle-size maximize stays a valid box for a window dragged off the right/bottom edge\n");

  {
    static test_task_t tc_v;
    wuss_task_t       *delegate_v;
    box_t              box_v, vb, titlebar, toggle;
    wuss_window_t     *win_v;
    int                outline_px, titlebar_height, inset, icon;
    int                cx, cy;

    memset(&tc_v, 0, sizeof(tc_v));
    tc_v.veto_pre_close = 1; /* this test pokes furniture with a window in a
                              * deliberately broken (inverted) box; a stray
                              * hit on the close icon must not tear win_v down
                              * mid-test */
    delegate_v = mk_task(wuss, test_handle, &tc_v);
    if (delegate_v == NULL) goto Failure;

    box_v.x0 = 10; box_v.y0 = 10;
    box_v.x1 = 70; box_v.y1 = 70; /* 60x60 content -- wide enough titlebar that
                                    * back/close/toggle icons don't overlap
                                    * (a titlebar much narrower than that makes
                                    * the toggle icon's box overlap close's, so
                                    * a click meant for toggle lands on close
                                    * instead, since hit-test checks close
                                    * first -- a separate, pre-existing issue
                                    * unrelated to toggle-size specifically).
                                    * Doc big enough that maximize is
                                    * screen-limited, not doc-limited. */
    rc = wuss_window_create(delegate_v, &box_v, "V", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_v);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* drag well past the right/bottom screen edge (200x200 screen) -- an
     * ordinary, already-supported position (see the drag-off-screen test
     * above): titlebar drag calls wuss_window_move with no clamping. From
     * here, "available space to the screen edge" (scr_width - visible.x0 -
     * furniture) goes negative, further than furniture alone can absorb. */
    wuss_window_move(win_v, POINT(310, 310));

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_v, &vb);
    titlebar.x0 = vb.x0 + outline_px;
    titlebar.x1 = vb.x1 - outline_px;
    titlebar.y0 = vb.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* V's toggle-size icon: maximize while off-screen */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure; /* a naive "space from x0 to the screen edge" sum goes
                      * negative from off-screen, producing an inverted box --
                      * un-hit-testable forever after, since box_contains_point
                      * can never match x0>x1: the window would be stuck,
                      * unclickable, unclosable */
    if (vb.x0 < 0 || vb.y0 < 0 || vb.x1 > 200 || vb.y1 > 200)
      goto Failure; /* toggle repositions toward the origin by the minimum
                      * needed, so even a window dragged right off the screen
                      * comes back fully on-screen when maximized */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. With the window clipped this narrow its toggle
     * icon overlaps close's box (see the box_v comment above), so the click
     * meant for toggle may land on close instead; hit-test checks close
     * first. That routes through wuss_window_try_close, and this task's
     * PRE_CLOSE veto (set at task creation) both keeps the window alive and
     * makes the click return the veto's non-OK -- accept that here. The
     * point of the check is line-below: the box must still be valid. */
    wuss_window_get_visible_bounds(win_v, &vb);
    titlebar.x0 = vb.x0 + outline_px;
    titlebar.x1 = vb.x1 - outline_px;
    titlebar.y0 = vb.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* V's toggle-size icon: shrink back */
    if (rc != result_OK && rc != result_BAD_ARG)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK && rc != result_BAD_ARG)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_v);
  }

  printf("test: a toggle-size that repositions the window blits the overlapping content rather than repainting it all, and restores the exact pre-toggle box\n");

  {
    wuss_task_t   *delegate_p;
    box_t          box_p, before, after, titlebar, toggle;
    wuss_window_t *win_p;
    int            outline_px, titlebar_height, inset, icon, cx, cy;
    int            i, dx, dy, kept_x, kept_y, kept_dirty;
    int            ghost_x, ghost_y, ghost_dirty;

    delegate_p = mk_task(wuss, NULL, NULL);
    if (delegate_p == NULL) goto Failure;

    box_p.x0 = 110; box_p.y0 = 110;
    box_p.x1 = 190; box_p.y1 = 190; /* 80x80 content, well down the bottom-right
                                      * of the 200x200 screen -- big enough to
                                      * have a real interior the blit can keep;
                                      * doc big enough that maximize is
                                      * screen-limited so the window must move */
    rc = wuss_window_create(delegate_p, &box_p, "P", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_p);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_p, &before);
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_p)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_p, &after);
    if (after.x0 >= before.x0 || after.y0 >= before.y0)
      goto Failure; /* the window had to move toward the origin to fit its
                      * maximized box on screen */
    if (after.x0 < 0 || after.y0 < 0 || after.x1 > 200 || after.y1 > 200)
      goto Failure; /* ...but only as far as needed, and it stays on-screen */
    if (after.x1 - after.x0 < 190 || after.y1 - after.y0 < 190)
      goto Failure; /* it genuinely fills (nearly) the whole screen */

    dx = after.x0 - before.x0;
    dy = after.y0 - before.y0;

    /* a point in the old content interior, clear of outline/titlebar/
     * scrollbar furniture on every side: after the toggle's shift by
     * (dx,dy) it lands in the new content interior, and the blit must have
     * carried its pixels there -- so it must NOT be in the dirty list.
     * (This window starts screen-clamped hard against the bottom-right, so
     * the maximized footprint is a superset of the old one -- there is no
     * vacated backdrop strip to check, only the preserved interior.) */
    kept_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2 + dx;
    kept_y = (before.y0 + outline_px + titlebar_height
              + before.y1 - outline_px - icon) / 2 + dy;

    /* where the OLD vertical scrollbar column lands once shifted by (dx,dy):
     * a whole-footprint blit would drag that stale scrollbar strip here,
     * into what is now interior content. The blit must be content-only, so
     * this point must be in the dirty list to be repainted as content. */
    ghost_x = before.x1 - outline_px - icon / 2 + dx;
    ghost_y = (before.y0 + outline_px + titlebar_height
               + before.y1 - outline_px) / 2 + dy;

    kept_dirty  = 0;
    ghost_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, kept_x, kept_y))
        kept_dirty = 1;
      if (box_contains_point(&region, ghost_x, ghost_y))
        ghost_dirty = 1;
    }
    if (kept_dirty)
      goto Failure; /* the shifted interior content was blitted, not repainted */
    if (!ghost_dirty)
      goto Failure; /* the old scrollbar's shifted position must repaint as
                      * content -- the blit must not have carried furniture */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: the icon moved with the grown/repositioned titlebar */
    wuss_window_get_visible_bounds(win_p, &after);
    titlebar.x1 = after.x1 - outline_px;
    titlebar.y0 = after.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK || hit != win_p)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_p, &after);
    if (after.x0 != before.x0 || after.y0 != before.y0 ||
        after.x1 != before.x1 || after.y1 != before.y1)
      goto Failure; /* restore returns the exact pre-toggle box, position too */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_p);
  }

  printf("test: toggle-size fills the screen for a window with no document extent (doc == 0), and per-axis where only one axis has a doc\n");

  {
    wuss_task_t   *delegate_z;
    box_t          box_z, before, after, titlebar, toggle;
    wuss_window_t *win_z;
    int            outline_px, titlebar_height, inset, icon, cx, cy, axis;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    /* axis 0: doc == (0,0) -> both axes fill the screen.
     * axis 1: doc == (60,0) -> width caps at ~60, height fills the screen. */
    for (axis = 0; axis < 2; axis++)
    {
      delegate_z = mk_task(wuss, NULL, NULL);
      if (delegate_z == NULL) goto Failure;

      box_z.x0 = 20; box_z.y0 = 20;
      box_z.x1 = 60; box_z.y1 = 60; /* 40x40 content */
      rc = wuss_window_create(delegate_z, &box_z, "Z", wuss_WINDOW_DEFAULT,
                              wuss_NO_BACKDROP,
                              axis == 0 ? SIZE2D(0, 0) : SIZE2D(60, 0),
                              SIZE2D(0, 0), &win_z);
      if (rc != result_OK)
        goto Failure;

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_visible_bounds(win_z, &before);
      titlebar.x1 = before.x1 - outline_px;
      titlebar.y0 = before.y0 + outline_px;
      toggle.x1 = titlebar.x1 - inset;
      toggle.x0 = toggle.x1 - icon;
      toggle.y0 = titlebar.y0 + inset;
      toggle.y1 = toggle.y0 + icon;
      cx = (toggle.x0 + toggle.x1) / 2;
      cy = (toggle.y0 + toggle.y1) / 2;

      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
      if (rc != result_OK || hit != win_z)
        goto Failure;
      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_content_bounds(win_z, &content);
      width  = content.x1 - content.x0;
      height = content.y1 - content.y0;

      if (height < 150)
        goto Failure; /* the y axis has no doc cap in either case -> fills the
                        * screen, not stuck at the 40px starting height */
      if (axis == 0 && width < 150)
        goto Failure; /* doc == 0 -> the x axis fills the screen too */
      if (axis == 1 && width != 60)
        goto Failure; /* doc.w == 60 -> the x axis caps at the doc extent */

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      /* toggle back restores the starting box */
      wuss_window_get_visible_bounds(win_z, &after);
      titlebar.x1 = after.x1 - outline_px;
      titlebar.y0 = after.y0 + outline_px;
      toggle.x1 = titlebar.x1 - inset;
      toggle.x0 = toggle.x1 - icon;
      toggle.y0 = titlebar.y0 + inset;
      toggle.y1 = toggle.y0 + icon;
      cx = (toggle.x0 + toggle.x1) / 2;
      cy = (toggle.y0 + toggle.y1) / 2;

      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
      if (rc != result_OK || hit != win_z)
        goto Failure;
      rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_visible_bounds(win_z, &after);
      if (after.x0 != before.x0 || after.y0 != before.y0 ||
          after.x1 != before.x1 || after.y1 != before.y1)
        goto Failure;

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_close(win_z);
    }
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_toggle_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_toggle_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
