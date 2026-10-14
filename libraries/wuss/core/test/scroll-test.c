/* core/test/scroll-test.c -- wuss scroll blit and repaint tests */

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

result_t wuss_scroll_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_window_set_scroll on a non-topmost window blits the un-occluded content and never dirties the occluder\n");

  {
    /* A window covered on its right half by another still blits its
     * un-occluded left half on a programmatic scroll. The right half shows
     * the occluder's own correct pixels, so it must stay clean -- dirtying
     * it would redraw the occluding window. Only the un-occluded
     * newly-exposed edge strip needs a repaint. */
    static test_task_t tc_s, tc_o;
    wuss_task_t       *delegate_s, *delegate_o;
    box_t              box_s, box_o, content;
    wuss_window_t     *win_s, *win_o;
    int                i, split_x;
    int                occluded_x, occluded_y, kept_x, kept_y, strip_x, strip_y;
    int                occluded_dirty, kept_dirty, strip_dirty;

    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 110; box_s.y1 = 90; /* 100x80 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 200), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_s, &content);
    split_x = (content.x0 + content.x1) / 2;

    box_o.x0 = split_x; box_o.y0 = content.y0 - 5;
    box_o.x1 = content.x1 + 40; box_o.y1 = content.y1 + 40;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    /* kept: un-occluded, near the bottom -- content there just slides up by
     * the scroll delta, so the blit must reuse it. occluded: under O -- must
     * stay clean. strip: un-occluded, in the newly-exposed bottom band --
     * must be repainted. */
    kept_x     = content.x0 + 8;
    kept_y     = content.y0 + 8;
    occluded_x = split_x + 8;
    occluded_y = content.y1 - 20;
    strip_x    = content.x0 + 8;
    strip_y    = content.y1 - 6;

    wuss_window_set_scroll(win_s, POINT(0, 12));

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    kept_dirty     = 0;
    occluded_dirty = 0;
    strip_dirty    = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, kept_x, kept_y))
        kept_dirty = 1;
      if (box_contains_point(&region, occluded_x, occluded_y))
        occluded_dirty = 1;
      if (box_contains_point(&region, strip_x, strip_y))
        strip_dirty = 1;
    }

    if (occluded_dirty)
      goto Failure; /* under the occluder: its pixels are already correct,
                      * dirtying this would redraw the occluding window */

    if (kept_dirty)
      goto Failure; /* un-occluded content must be reused by the blit */

    if (!strip_dirty)
      goto Failure; /* the un-occluded newly-exposed strip must be repainted */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_o);
    wuss_window_close(win_s);
  }

  printf("test: wuss_window_set_scroll never blits a scrolled window's content over a mid-content occluder\n");

  {
    /* Regression: a small window O sits in the middle of a larger scrollable
     * window M's content, covering neither M's top nor bottom edge. A
     * programmatic vertical scroll of M takes a blit source slice from below
     * O and shifts it up by the scroll delta, so its destination overlaps O.
     * screen_copy_rect only clips to the content box, so without the fix it
     * paints M's content over O and marks that area "copied" -- excluding it
     * from the repaint set, so O's pixels stay overpainted with M's colour.
     * With the fix each destination is clipped against the occluders and only
     * the un-occluded sub-pieces are blitted, leaving O's own pixels intact.
     * Read the framebuffer behind O to check. */
    colour_t           cm;
    uint32_t           fb_m, fb_o;
    static test_task_t tc_m, tc_o;
    wuss_task_t       *delegate_m, *delegate_o;
    box_t              box_m, box_o, content;
    wuss_window_t     *win_m, *win_o;
    int                occ_x, occ_y, mid_x, mid_y, delta;

    cm = colour_rgb(0x11, 0x22, 0x33);
    tc_m.redraw_count = 0; tc_m.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_m = mk_task(wuss, paint_handle, &cm);
    if (delegate_m == NULL) goto Failure;
    delegate_o = mk_task(wuss, paint_handle, NULL);
    if (delegate_o == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 110; box_m.y1 = 110; /* 100x100 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 300), SIZE2D(0, 0), &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);

    /* occluder spanning M's full content width, a band in the vertical
     * middle -- so wuss__clip_to_visible splits M's blit source into a top
     * and a bottom band with nothing behind O. Scrolling down slides the
     * bottom band up; part of its destination lands behind O, yet no dirty
     * repaint region touches O, so without the fix O stays overpainted. */
    box_o.x0 = content.x0 - 5;  box_o.y0 = content.y0 + 40;
    box_o.x1 = content.x1 + 5;  box_o.y1 = content.y0 + 70;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates: M then O paint */
    if (rc != result_OK)
      goto Failure;

    /* sample the framebuffer: occ_* sits behind O, mid_* sits in M's content
     * clear of O -- record what each colour actually renders as */
    occ_x = (box_o.x0 + box_o.x1) / 2;
    occ_y = box_o.y1 - 3; /* near O's bottom: the bottom band's blit
                           * destination reaches up to here */
    mid_x = content.x0 + 5;
    mid_y = content.y0 + 5;
    fb_o  = ((const uint32_t *) fixture.pixels)[occ_y * 200 + occ_x];
    fb_m  = ((const uint32_t *) fixture.pixels)[mid_y * 200 + mid_x];
    if (fb_o == fb_m)
      goto Failure; /* the two windows must render distinguishable pixels */

    delta = 20;
    wuss_window_set_scroll(win_m, POINT(0, delta));
    rc = wuss_redraw_dirty(wuss); /* apply the scroll's blit + any repaint */
    if (rc != result_OK)
      goto Failure;

    /* the pixel behind O must still be O's, not M's content blitted over it */
    if (((const uint32_t *) fixture.pixels)[occ_y * 200 + occ_x] != fb_o)
      goto Failure;

    wuss_window_close(win_o);
    wuss_window_close(win_m);
  }

  printf("test: wuss_window_set_scroll orders its blit sub-pieces so one never clobbers another's source\n");

  {
    /* Regression: a small window O floats in the middle of a larger
     * scrollable window M, with a gap all round it. wuss__clip_to_visible
     * carves M's blittable content into bands around O; a vertical scroll
     * shifts each band by the same delta, and one band's shifted
     * destination lands on another band's still-unread source. Blitting the
     * bands in clip-emit order corrupts M's own content -- pixels near O's
     * bottom edge end up double-shifted. The blit must be ordered (or fall
     * back). paint_handle paints M as one-pixel rows whose blue channel
     * encodes document Y, so a mis-shifted row is detectable exactly. */
    colour_t           cm;
    static test_task_t tc_m, tc_o;
    wuss_task_t       *delegate_m, *delegate_o;
    box_t              box_m, box_o, content, ovis;
    wuss_window_t     *win_m, *win_o;
    int                gx, gy, delta, bad;

    cm = colour_rgb(0x11, 0x22, 0x33);
    tc_m.redraw_count = 0; tc_m.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_m = mk_task(wuss, paint_handle, &cm);
    if (delegate_m == NULL) goto Failure;
    delegate_o = mk_task(wuss, paint_handle, NULL);
    if (delegate_o == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 110; box_m.y1 = 110; /* 100x100 content; doc taller, scrollable */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 300), SIZE2D(0, 0), &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);

    /* O strictly inside M's content, gap on every side */
    box_o.x0 = content.x0 + 25; box_o.y0 = content.y0 + 25;
    box_o.x1 = content.x0 + 75; box_o.y1 = content.y0 + 65;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    /* probe the gap column just right of O: pure M content, must slide up by
     * exactly the scroll delta with no discontinuity */
    wuss_window_get_visible_bounds(win_o, &ovis);
    gx    = (ovis.x1 + content.x1) / 2;
    delta = 20;

    wuss_window_set_scroll(win_m, POINT(0, delta));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every row in the gap column, across O's vertical span, must show the
     * document Y that scrolling put there: doc_y = (screen_y - content.y0) +
     * delta, low byte carried in blue by paint_handle */
    bad = 0;
    for (gy = ovis.y0 + 2; gy < ovis.y1 - 2; gy++)
    {
      uint32_t px, blue;

      if (gy < content.y0 || gy >= content.y1)
        continue;
      px   = ((const uint32_t *) fixture.pixels)[gy * 200 + gx];
      blue = px & 0xff;
      if (blue != (uint32_t) ((gy - content.y0 + delta) & 0xff))
        bad = 1;
    }
    if (bad)
      goto Failure; /* a band blit clobbered another band's source */

    wuss_window_close(win_o);
    wuss_window_close(win_m);
  }

  printf("test: two wheel scrolls delivered before wuss_redraw_dirty runs never blit the first scroll's undrawn strip into the second scroll's content\n");

  {
    /* Regression: fast wheel spinning can call wuss_window_set_scroll twice
     * (or more) before wuss_redraw_dirty ever runs. The first call's blit
     * shifts old content and marks the newly-exposed strip dirty but does
     * NOT paint it -- that strip holds pre-scroll pixels until the redraw
     * runs. If the second call's blit source includes that still-dirty
     * strip, it smears those stale pixels across the content box instead of
     * being excluded per the "stale" collection in wuss_window_set_scroll.
     * paint_handle paints one-pixel rows keyed by doc_y in the blue channel,
     * so a stale/smeared row is detectable exactly after a single final
     * redraw. */
    colour_t           cw;
    static test_task_t tc_w;
    wuss_task_t       *delegate_w;
    box_t              box_w, content_w;
    wuss_window_t     *win_w;
    int                wx, wy, bad;

    cw = colour_rgb(0x55, 0x66, 0x77);
    tc_w.redraw_count = 0; tc_w.mouse_count = 0;
    delegate_w = mk_task(wuss, paint_handle, &cw);
    if (delegate_w == NULL) goto Failure;

    box_w.x0 = 10; box_w.y0 = 10;
    box_w.x1 = 90; box_w.y1 = 90; /* 80x80 content; doc taller, scrollable */
    rc = wuss_window_create(delegate_w, &box_w, "W", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 200), SIZE2D(0, 0), &win_w);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_w, &content_w);

    /* two wheel notches back-to-back, no redraw in between -- exactly what
     * fast spinning delivers */
    wuss_window_set_scroll(win_w, POINT(0, 10));
    wuss_window_set_scroll(win_w, POINT(0, 20));

    rc = wuss_redraw_dirty(wuss); /* the only flush, as fast scrolling leaves it */
    if (rc != result_OK)
      goto Failure;

    /* every row of the content box must show the doc_y the final scroll
     * offset (20) puts there -- no row may still carry pixels from the
     * intermediate offset (10) or from before either scroll */
    bad = 0;
    wx  = (content_w.x0 + content_w.x1) / 2;
    for (wy = content_w.y0; wy < content_w.y1; wy++)
    {
      uint32_t px, blue;

      px   = ((const uint32_t *) fixture.pixels)[wy * 200 + wx];
      blue = px & 0xff;
      if (blue != (uint32_t) ((wy - content_w.y0 + 20) & 0xff))
        bad = 1;
    }
    if (bad)
      goto Failure; /* a stacked scroll's blit reused a stale strip */

    wuss_window_close(win_w);
  }

  printf("test: scrolling a window shrunk below its document size never marks a neighbour's screen area dirty\n");

  {
    /* Regression: a window resized down below its document extent (so it
     * becomes scrollable), then scrolled by wheel/scrollbar, must not touch
     * a neighbour window's own screen area. flood_full_bounds_handle ignores
     * "clip" entirely, mirroring a real task (e.g. saturn.c) that redraws
     * its full sample space every call rather than honouring clip -- the
     * wuss-core clip machinery, not the task, must be what keeps such a
     * task's pixels inside its own window. */
    colour_t           s_colour, n_colour;
    static test_task_t tc_s, tc_n;
    wuss_task_t       *delegate_s, *delegate_n;
    box_t              box_s, box_n, content_n;
    wuss_window_t     *win_s, *win_n;
    int                nx, ny, bad;

    s_colour = colour_rgb(0xcc, 0xdd, 0xee);
    n_colour = colour_rgb(0x40, 0x60, 0x80);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    tc_n.redraw_count = 0; tc_n.mouse_count = 0;
    delegate_s = mk_task(wuss, flood_full_bounds_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;
    delegate_n = mk_task(wuss, flood_full_bounds_handle, &n_colour);
    if (delegate_n == NULL) goto Failure;

    /* S starts full-sized (matches its document, so no scrollbars yet), then
     * gets shrunk -- exercising wuss_window_resize's scroll-reclamp path,
     * distinct from creating it small to begin with. */
    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 90; box_s.y1 = 90;
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 80), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    /* N sits immediately to the right of S's outer box, close enough that a
     * blit destination sliding even a few pixels past S's own bounds lands
     * on N. */
    box_n.x0 = box_s.x1 + 2; box_n.y0 = 10;
    box_n.x1 = box_n.x0 + 40; box_n.y1 = 90;
    rc = wuss_window_create(delegate_n, &box_n, "N", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_n.x1 - box_n.x0, box_n.y1 - box_n.y0),
                            SIZE2D(0, 0), &win_n);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    /* shrink S well below its 80x80 document -- this window is now
     * scrollable */
    rc = wuss_window_resize(win_s, SIZE2D(30, 30));
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* scroll it, as a wheel/scrollbar-drag would */
    wuss_window_set_scroll(win_s, POINT(20, 20));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every pixel of N's own content (not its titlebar/outline chrome) must
     * still show exactly N's own flood colour */
    wuss_window_get_content_bounds(win_n, &content_n);
    bad = 0;
    for (ny = content_n.y0; ny < content_n.y1 && !bad; ny++)
      for (nx = content_n.x0; nx < content_n.x1; nx++)
      {
        uint32_t px;

        px = ((const uint32_t *) fixture.pixels)[ny * 200 + nx];
        if ((px & 0xffffff) != 0x406080)
        {
          bad = 1;
          break;
        }
      }
    if (bad)
      goto Failure; /* S's scroll blit painted over N's screen area */

    wuss_window_close(win_n);
    wuss_window_close(win_s);
  }

  printf("test: wuss_window_invalidate_visible on a shrunk, scrolled window only dirties its own visible box\n");

  {
    /* Regression: wuss_window_invalidate(window, NULL) built its "whole
     * visible rect" box already in screen space (offset by content.x0/y0),
     * then translated it a second time by "content.x0 - scroll", so a
     * scrolled window's dirty rect ended up shifted by -scroll off to one
     * side of the window rather than sitting over it -- e.g. saturn.c's
     * per-idle-tick wuss_window_invalidate_visible(), once its window was
     * shrunk below its document and then scrolled, queued a dirty rect
     * outside the window's own screen area (fill_backdrop_excluding_content
     * then repaints that stray area as bare desktop every frame). */
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, dirty;
    wuss_window_t     *win_s;
    colour_t           s_colour;

    s_colour = colour_rgb(0xcc, 0xdd, 0xee);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    delegate_s = mk_task(wuss, flood_full_bounds_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 60; box_s.y0 = 60;
    box_s.x1 = 140; box_s.y1 = 140;
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 80), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss); /* flush the create */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_window_resize(win_s, SIZE2D(30, 30));
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_s, POINT(20, 20));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* mirrors saturn_idle: invalidate the whole visible rect with no
     * explicit local_box, exactly what a task's idle handler calls every
     * frame -- inspect the queued dirty rect directly rather than via
     * pixels, since every window (including S itself) re-clips to its own
     * content box on redraw regardless of what the dirty list says, so a
     * stray dirty rect only ever shows up as a wasted backdrop repaint, not
     * as a colour mismatch. */
    wuss_window_invalidate_visible(win_s);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;
    wuss_get_dirty(wuss, 0, &dirty);
    if (!box_contains_box(&dirty, &win_s->visible))
      goto Failure; /* dirty rect strayed outside S's own visible box */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_s);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_scroll_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_scroll_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
