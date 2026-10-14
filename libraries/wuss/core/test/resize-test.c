/* core/test/resize-test.c -- wuss resize blit and sliver tests */

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

/* Total area covered by the dirty list, counting overlapped pixels once.
 * Summing each region's area instead would double-count wherever two
 * invalidations overlap, which they legitimately do. */
static int dirty_union_area(wuss_t *wuss, const box_t *bounds)
{
  static unsigned char covered[512 * 512];

  box_t                region;
  int                  w, h, i, x, y, area;

  w = bounds->x1 - bounds->x0;
  h = bounds->y1 - bounds->y0;
  if (w <= 0 || h <= 0 || w > 512 || h > 512)
    return -1;

  memset(covered, 0, (size_t) w * h);

  for (i = 0; i < wuss_get_dirty_count(wuss); i++)
  {
    wuss_get_dirty(wuss, i, &region);
    for (y = MAX(region.y0, bounds->y0); y < MIN(region.y1, bounds->y1); y++)
      for (x = MAX(region.x0, bounds->x0); x < MIN(region.x1, bounds->x1); x++)
        covered[(y - bounds->y0) * w + (x - bounds->x0)] = 1;
  }

  area = 0;
  for (i = 0; i < w * h; i++)
    area += covered[i];

  return area;
}

result_t wuss_resize_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;
  wuss_window_t *hit;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: wuss_window_resize on a topmost window at max scroll blits the still-valid content rather than redrawing it all\n");

  {
    /* Unlike the toggle-size path above, a direct wuss_window_resize (the
     * interactive drag-resize path) runs with the screen still showing this
     * window's content at the old geometry and old scroll offset. When the
     * grow forces a scroll re-clamp, the overlap of the old and new content
     * boxes is genuine on-screen content that just needs sliding by the
     * scroll delta -- only the newly-exposed strip needs a real repaint. */
    static test_task_t tc_d;
    wuss_task_t       *delegate_d;
    box_t              box_d, content_before;
    wuss_window_t     *win_d;
    point_t            scroll_d;
    int                i;
    int                top_x, top_y, mid_x, mid_y, bottom_x, bottom_y;
    int                top_dirty, mid_dirty, bottom_dirty;

    tc_d.redraw_count = 0;
    tc_d.mouse_count  = 0;
    delegate_d = mk_task(wuss, test_handle, &tc_d);
    if (delegate_d == NULL) goto Failure;

    box_d.x0 = 10; box_d.y0 = 10;
    box_d.x1 = 90; box_d.y1 = 90; /* 80x80 content; doc taller, so it starts scrollable */
    rc = wuss_window_create(delegate_d, &box_d, "D", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 140), SIZE2D(0, 0), &win_d);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_d, &content_before);

    /* scroll to the bottom: max_y = doc.h (140) - content height (80) = 60 */
    wuss_window_set_scroll(win_d, POINT(0, 60));
    rc = wuss_redraw_dirty(wuss); /* flush the scroll's own invalidate */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 60)
      goto Failure;

    /* Three probe points at the same x, well clear of the scrollbar column.
     * The re-clamp drops scroll.y from 60 to 0, so on-screen content slides
     * DOWN by 60px: the top ~60px of the content box is newly revealed and
     * must be repainted, everything below it was already on screen and must
     * be reused by the blit. */
    top_x    = content_before.x0 + 8;
    top_y    = content_before.y0 + 8;   /* < 60px down: in the revealed strip */
    mid_x    = content_before.x0 + 8;
    mid_y    = content_before.y0 + 70;  /* > 60px down: reused */
    bottom_x = content_before.x0 + 8;
    bottom_y = content_before.y1 - 8;   /* near old viewport bottom: reused */

    /* grow the content taller than the doc: content height goes to >= 140,
     * so max_y drops to 0 and scroll.y is clamped back from 60 to 0 */
    rc = wuss_window_resize(win_d, SIZE2D(80, 160));
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 0)
      goto Failure; /* the grow must have re-clamped the offset */

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    top_dirty    = 0;
    mid_dirty    = 0;
    bottom_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, top_x, top_y))
        top_dirty = 1;
      if (box_contains_point(&region, mid_x, mid_y))
        mid_dirty = 1;
      if (box_contains_point(&region, bottom_x, bottom_y))
        bottom_dirty = 1;
    }

    if (!top_dirty)
      goto Failure; /* the newly-revealed strip must be repainted */

    if (mid_dirty || bottom_dirty)
      goto Failure; /* content that stayed on screen must be reused by the
                      * blit-and-shift, not redrawn -- a full content-box
                      * invalidate has regressed the optimisation */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_d);
  }

  printf("test: wuss_window_resize on a non-topmost window blits the un-occluded content and never dirties the occluder\n");

  {
    /* Another window covers the right half of D's content. The blit source
     * is the old content box MINUS what the occluder covered, so the
     * un-occluded left half is slid into place; the right half still shows
     * the occluder's own correct pixels, so it must stay clean -- dirtying
     * it would redraw the occluding window for nothing. Only the un-occluded
     * newly-revealed top strip needs a repaint. */
    static test_task_t tc_d, tc_o;
    wuss_task_t       *delegate_d, *delegate_o;
    box_t              box_d, box_o, content_before;
    wuss_window_t     *win_d, *win_o;
    point_t            scroll_d;
    int                i, split_x;
    int                occluded_x, occluded_y, kept_x, kept_y, strip_x, strip_y;
    int                occluded_dirty, kept_dirty, strip_dirty;

    tc_d.redraw_count = 0; tc_d.mouse_count = 0;
    tc_o.redraw_count = 0; tc_o.mouse_count = 0;
    delegate_d = mk_task(wuss, test_handle, &tc_d);
    if (delegate_d == NULL) goto Failure;
    delegate_o = mk_task(wuss, test_handle, &tc_o);
    if (delegate_o == NULL) goto Failure;

    box_d.x0 = 10; box_d.y0 = 10;
    box_d.x1 = 110; box_d.y1 = 90; /* 100x80 content; doc taller, so scrollable */
    rc = wuss_window_create(delegate_d, &box_d, "D", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(100, 140), SIZE2D(0, 0), &win_d);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_d, &content_before);
    split_x = (content_before.x0 + content_before.x1) / 2;

    /* occluder covering the right half of D's content box and beyond */
    box_o.x0 = split_x; box_o.y0 = content_before.y0 - 5;
    box_o.x1 = content_before.x1 + 40; box_o.y1 = content_before.y1 + 40;
    rc = wuss_window_create(delegate_o, &box_o, "O", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(box_o.x1 - box_o.x0, box_o.y1 - box_o.y0),
                            SIZE2D(0, 0), &win_o);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates */
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_d, POINT(0, 60)); /* max_y = 140 - 80 = 60 */
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 60)
      goto Failure;

    /* occluded: under O -- must stay clean. kept: un-occluded, below the
     * ~60px revealed strip -- reused by the blit, must stay clean. strip:
     * un-occluded, in the newly-revealed top band -- must be repainted. */
    occluded_x = split_x + 8;
    occluded_y = content_before.y1 - 8;
    kept_x     = content_before.x0 + 8;
    kept_y     = content_before.y1 - 8;
    strip_x    = content_before.x0 + 8;
    strip_y    = content_before.y0 + 8;

    rc = wuss_window_resize(win_d, SIZE2D(100, 160)); /* grow past doc height */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_scroll(win_d, &scroll_d);
    if (scroll_d.y != 0)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    occluded_dirty = 0;
    kept_dirty     = 0;
    strip_dirty    = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      box_t region;

      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, occluded_x, occluded_y))
        occluded_dirty = 1;
      if (box_contains_point(&region, kept_x, kept_y))
        kept_dirty = 1;
      if (box_contains_point(&region, strip_x, strip_y))
        strip_dirty = 1;
    }

    if (occluded_dirty)
      goto Failure; /* under the occluder: its pixels are already correct,
                      * dirtying this would redraw the occluding window */

    if (kept_dirty)
      goto Failure; /* un-occluded content must be reused by the blit */

    if (!strip_dirty)
      goto Failure; /* the un-occluded newly-revealed strip must be repainted */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_o);
    wuss_window_close(win_d);
  }

  printf("test: shrinking a window repaints its content right up to the new content box's own edge\n");

  {
    /* Regression: dragging a resize handle to make a window smaller must
     * leave no stale sliver between the client's repainted content and the
     * furniture (scrollbar/outline) now sitting at the new, smaller edge. */
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, content_s;
    wuss_window_t     *win_s;
    colour_t           s_colour;
    int                nx, ny, bad;

    s_colour = colour_rgb(0xcc, 0xdd, 0xee);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    delegate_s = mk_task(wuss, flood_full_bounds_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 90; box_s.y1 = 90;
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(80, 80), SIZE2D(200, 200), &win_s);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss); /* flush the create, paint S's initial content */
    if (rc != result_OK)
      goto Failure;

    /* scroll before shrinking -- the shrink must not leave a stale sliver
     * at the new content edge regardless of scroll offset */
    wuss_window_set_scroll(win_s, POINT(20, 20));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* an interactive drag delivers many small pointer-move events, each
     * calling wuss_window_resize, before the display ever gets to redraw --
     * shrink one pixel at a time with no flush between them, matching that;
     * throw in an overshoot-and-correct wobble too, as a real drag does */
    {
      int step, w;

      for (step = 0, w = 80; w > 30; w--, step++)
      {
        rc = wuss_window_resize(win_s, SIZE2D(w, w));
        if (rc != result_OK)
          goto Failure;
      }
      rc = wuss_window_resize(win_s, SIZE2D(35, 35)); /* overshoot back out */
      if (rc != result_OK)
        goto Failure;
      rc = wuss_window_resize(win_s, SIZE2D(30, 30)); /* settle */
      if (rc != result_OK)
        goto Failure;
    }
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* every pixel of S's own (new, smaller) content box must show S's
     * flood colour right up to content_s.x1-1 / content_s.y1-1 -- no gap
     * between the repainted content and the furniture now abutting it */
    wuss_window_get_content_bounds(win_s, &content_s);
    bad = 0;
    for (ny = content_s.y0; ny < content_s.y1 && !bad; ny++)
      for (nx = content_s.x0; nx < content_s.x1; nx++)
      {
        uint32_t px;

        px = ((const uint32_t *) fixture.pixels)[ny * 200 + nx];
        if ((px & 0xffffff) != 0xccddee)
        {
          bad = 1;
          break;
        }
      }
    if (bad)
      goto Failure; /* stale sliver left along the new content box's edge */

    wuss_window_close(win_s);
  }

  printf("test: a fast resize-grow past the scroll clamp never blits a not-yet-painted sliver\n");

  {
    /* Regression: wuss_window_resize's scroll-reclamp blit (window scrolled
     * to the document's bottom/right, then grown so the bigger viewport
     * needs less scroll and the clamp pulls the offset back) reads its blit
     * source straight from wuss__clip_to_visible over the *old* content box,
     * unlike wuss_window_move/wuss_window_set_scroll which both also strip
     * anything still sitting in wuss->dirty.box[] (queued by an earlier call this
     * same frame, not yet painted). An interactive drag delivers several
     * resize calls before a redraw ever runs: one grow queues its
     * newly-exposed sliver dirty without painting it; that sliver sits
     * inside the *next* call's "old content box", so the next call's
     * scroll-reclamp blit slides it as if it were this window's own settled
     * rendering -- smearing a stale/undrawn sliver that no later invalidate
     * ever repaints. paint_handle's per-row blue-encoded doc_y makes a
     * wrongly-slid row detectable exactly. */
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, content_s;
    wuss_window_t     *win_s;
    colour_t           s_colour;
    point_t            scroll;
    int                sy, sx, bad;

    s_colour = colour_rgb(0x11, 0x22, 0x33);
    tc_s.redraw_count = 0; tc_s.mouse_count = 0;
    delegate_s = mk_task(wuss, paint_handle, &s_colour);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 40; box_s.y1 = 40; /* 30x30 content; a doc far bigger than the
                                   * screen so growing 1px at a time pulls the
                                   * scroll clamp back by exactly 1px per
                                   * call, keeping it non-zero (and so
                                   * re-triggering the reclamp path) across
                                   * the whole grow range below */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(30, 30), &win_s);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss); /* flush the create, paint S's initial content */
    if (rc != result_OK)
      goto Failure;

    /* scroll to the document's bottom-right: no further offset possible at
     * this size */
    wuss_window_set_scroll(win_s, POINT(970, 970));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* grow two calls at a time with no flush between them, as an interactive
     * drag delivers several pointer-move events per rendered frame -- each
     * bigger size pulls the scroll clamp back by 1px, so every single call
     * re-enters the reclamp path, and the second call of each pair reads its
     * blit source over ground the first call of the pair only just
     * invalidated (its own grown sliver) but never painted. Flushing after
     * every pair (rather than only once at the end) matches a real redraw
     * loop -- it also stops one pair's leftover full-footprint dirty rect
     * from coalescing over a later pair's corruption and masking it. */
    {
      int w;

      for (w = 30; w <= 90; w += 2)
      {
        rc = wuss_window_resize(win_s, SIZE2D(w, w));
        if (rc != result_OK)
          goto Failure;
        rc = wuss_window_resize(win_s, SIZE2D(w + 1, w + 1));
        if (rc != result_OK)
          goto Failure;

        rc = wuss_redraw_dirty(wuss);
        if (rc != result_OK)
          goto Failure;

        /* every row of S's current content box must show exactly the
         * document Y that its current scroll offset puts there -- a
         * stale-sliver blit leaves some row(s) showing a stale doc_y (or
         * whatever else was sitting in the framebuffer) instead */
        wuss_window_get_content_bounds(win_s, &content_s);
        wuss_window_get_scroll(win_s, &scroll);
        bad = 0;
        for (sy = content_s.y0; sy < content_s.y1 && !bad; sy++)
        {
          uint32_t px, blue, want;

          want = (uint32_t) ((sy - content_s.y0 + scroll.y) & 0xff);
          for (sx = content_s.x0; sx < content_s.x1; sx++)
          {
            px   = ((const uint32_t *) fixture.pixels)[sy * 200 + sx];
            blue = px & 0xff;
            if (blue != want)
            {
              bad = 1;
              break;
            }
          }
        }
        if (bad)
          goto Failure; /* a not-yet-painted sliver got blitted as if settled */
      }
    }

    wuss_window_close(win_s);
  }

  printf("test: a fast resize drag that jumps size and reverses direction never blits a not-yet-painted sliver\n");

  {
    /* Same hazard as the paired-grow test above, but matching a real
     * interactive drag more closely: the pointer can jump the size by a
     * large delta in one event (not just 1px), and can reverse from
     * growing to shrinking and back within the same unflushed batch. A
     * shrink never re-enters the reclamp path itself (it only ever raises
     * the clamp ceiling), but it still queues its own shrunk-away sliver
     * dirty -- and the very next call, if it grows back past the clamp,
     * must not treat that still-unpainted sliver as settled ground either. */
    static test_task_t tc_e;
    wuss_task_t       *delegate_e;
    box_t              box_e, content_e;
    wuss_window_t     *win_e;
    colour_t           e_colour;
    point_t            scroll;
    int                sy, sx, bad, step;
    int                sizes[] = { 30, 90, 35, 120, 31, 90, 30, 60, 110, 30 };

    e_colour = colour_rgb(0x11, 0x22, 0x33);
    tc_e.redraw_count = 0; tc_e.mouse_count = 0;
    delegate_e = mk_task(wuss, paint_handle, &e_colour);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 10; box_e.y0 = 10;
    box_e.x1 = 40; box_e.y1 = 40;
    rc = wuss_window_create(delegate_e, &box_e, "E", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(30, 30), &win_e);
    if (rc != result_OK)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_e, POINT(970, 970));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    for (step = 0; step < 9; step += 2)
    {
      rc = wuss_window_resize(win_e, SIZE2D(sizes[step], sizes[step]));
      if (rc != result_OK)
        goto Failure;
      rc = wuss_window_resize(win_e, SIZE2D(sizes[step + 1], sizes[step + 1]));
      if (rc != result_OK)
        goto Failure;

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_content_bounds(win_e, &content_e);
      wuss_window_get_scroll(win_e, &scroll);
      content_e.x1 = MIN(content_e.x1, 200);
      content_e.y1 = MIN(content_e.y1, 200);
#ifdef WUSS_ICONBAR
      /* clamp above the icon bar's fixed bottom strip too: it always draws
       * on top of every window, same reasoning as the occluder skip in the
       * next test below */
      content_e.y1 = MIN(content_e.y1, 200 - WUSS_ICONBAR_HEIGHT);
#endif
      bad = 0;
      for (sy = content_e.y0; sy < content_e.y1 && !bad; sy++)
      {
        uint32_t px, blue, want;

        want = (uint32_t) ((sy - content_e.y0 + scroll.y) & 0xff);
        for (sx = content_e.x0; sx < content_e.x1; sx++)
        {
          px   = ((const uint32_t *) fixture.pixels)[sy * 200 + sx];
          blue = px & 0xff;
          if (blue != want)
          {
            bad = 1;
            break;
          }
        }
      }
      if (bad)
        goto Failure; /* a not-yet-painted sliver got blitted as if settled */
    }

    wuss_window_close(win_e);
  }

  printf("test: a long unflushed batch of resize calls (matching a high-poll-rate mouse) never blits a not-yet-painted sliver\n");

  {
    /* The tests above batch only 1-2 resize calls per redraw; a real SDL
     * drag drains its whole event queue before ever calling
     * wuss_redraw_dirty (see apps/wuss/main.c's wuss_frame), so a fast
     * high-poll-rate mouse can deliver dozens of resize calls in one
     * batch. Stress that scale directly, using a cheap deterministic
     * pseudo-random walk rather than a fixed table, so the sequence covers
     * far more shapes (including piece-budget overflow) than a short
     * hand-picked list ever would. */
    static test_task_t tc_f, tc_occ;
    wuss_task_t       *delegate_f, *delegate_occ;
    box_t              box_f, box_occ, content_f;
    wuss_window_t     *win_f, *win_occ;
    colour_t           f_colour;
    point_t            scroll;
    unsigned int       seed;
    int                sy, sx, bad, batch, step, sizew, sizeh;

    f_colour = colour_rgb(0x44, 0x55, 0x66);
    tc_f.redraw_count = 0; tc_f.mouse_count = 0;
    delegate_f = mk_task(wuss, paint_handle, &f_colour);
    if (delegate_f == NULL) goto Failure;

    box_f.x0 = 10; box_f.y0 = 10;
    box_f.x1 = 40; box_f.y1 = 40;
    rc = wuss_window_create(delegate_f, &box_f, "F", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(30, 30), &win_f);
    if (rc != result_OK)
      goto Failure;

    /* An occluder created after (so it lands on top of) win_f, sat over its
     * corner: real desktops are rarely a single window, and
     * wuss__clip_to_visible's occlusion-carving path (untouched by every
     * test above) can only interact with the reclamp blit's stale-region
     * exclusion when there is something to occlude. New windows land at the
     * z_order head (topmost) -- see wuss_window_create. A NULL task_data
     * gives paint_handle's flat-fill branch, so the occluded area's colour
     * can be told apart from win_f's doc-row pattern by eye if this ever
     * fails; the pixel check below instead uses win_occ->visible (its real,
     * furniture-adjusted footprint) rather than the requested box, since
     * wuss__nudge_visible_onscreen/carve can move or grow it. */
    tc_occ.redraw_count = 0; tc_occ.mouse_count = 0;
    delegate_occ = mk_task(wuss, paint_handle, NULL);
    if (delegate_occ == NULL) goto Failure;

    box_occ.x0 = 25; box_occ.y0 = 25;
    box_occ.x1 = 60; box_occ.y1 = 60;
    rc = wuss_window_create(delegate_occ, &box_occ, "OCC", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(20, 20), SIZE2D(20, 20), &win_occ);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_set_scroll(win_f, POINT(970, 970));
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    seed  = 12345u;
    sizew = 30;
    sizeh = 30;
    for (batch = 0; batch < 20; batch++)
    {
      for (step = 0; step < 25; step++)
      {
        seed   = seed * 1103515245u + 12345u;
        sizew += (int) (seed >> 24) % 21 - 10; /* +/-10 px per call */
        seed   = seed * 1103515245u + 12345u;
        sizeh += (int) (seed >> 24) % 21 - 10;
        sizew  = CLAMP(sizew, 30, 150);
        sizeh  = CLAMP(sizeh, 30, 150);

        rc = wuss_window_resize(win_f, SIZE2D(sizew, sizeh));
        if (rc != result_OK)
          goto Failure;
      }

      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;

      wuss_window_get_content_bounds(win_f, &content_f);
      wuss_window_get_scroll(win_f, &scroll);
      content_f.x1 = MIN(content_f.x1, 200);
      content_f.y1 = MIN(content_f.y1, 200);
#ifdef WUSS_ICONBAR
      /* clamp above the icon bar's fixed bottom strip too: it always draws
       * on top of every window, same reasoning as the occluder skip below */
      content_f.y1 = MIN(content_f.y1, 200 - WUSS_ICONBAR_HEIGHT);
#endif
      bad = 0;
      for (sy = content_f.y0; sy < content_f.y1 && !bad; sy++)
      {
        uint32_t px, blue, want;

        want = (uint32_t) ((sy - content_f.y0 + scroll.y) & 0xff);
        for (sx = content_f.x0; sx < content_f.x1; sx++)
        {
          /* skip ground legitimately covered by the occluder's real,
           * furniture-adjusted footprint -- it shows win_occ's own colour
           * there, not win_f's doc-row pattern */
          if (sx >= win_occ->visible.x0 && sx < win_occ->visible.x1 &&
              sy >= win_occ->visible.y0 && sy < win_occ->visible.y1)
            continue;

          px   = ((const uint32_t *) fixture.pixels)[sy * 200 + sx];
          blue = px & 0xff;
          if (blue != want)
          {
            bad = 1;
            break;
          }
        }
      }
      if (bad)
        goto Failure; /* a not-yet-painted sliver got blitted as if settled */
    }

    wuss_window_close(win_occ);
    wuss_window_close(win_f);
  }

  printf("test: wuss_WINDOW_NO_RESIZE_BLIT redraws the whole window instead of blitting\n");

  {
    static test_task_t tc_nb;
    wuss_task_t       *delegate_nb;
    box_t              box_nb, before, titlebar, toggle;
    wuss_window_t     *win_nb;
    int                outline_px, titlebar_height, inset, icon;
    int                i, cx, cy, interior_x, interior_y, interior_dirty;

    tc_nb.redraw_count = 0;
    tc_nb.mouse_count  = 0;
    delegate_nb = mk_task(wuss, test_handle, &tc_nb);
    if (delegate_nb == NULL) goto Failure;

    box_nb.x0 = 10; box_nb.y0 = 10;
    box_nb.x1 = 50; box_nb.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(delegate_nb, &box_nb, "NB",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_nb);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the create's own invalidate */
    if (rc != result_OK)
      goto Failure;

    outline_px      = 1;
    titlebar_height = 20;
    inset           = 3;
    icon            = titlebar_height - 2 * inset;

    wuss_window_get_visible_bounds(win_nb, &before);
    titlebar.x0 = before.x0 + outline_px;
    titlebar.x1 = before.x1 - outline_px;
    titlebar.y0 = before.y0 + outline_px;
    toggle.x1 = titlebar.x1 - inset;
    toggle.x0 = toggle.x1 - icon;
    toggle.y0 = titlebar.y0 + inset;
    toggle.y1 = toggle.y0 + icon;
    cx = (toggle.x0 + toggle.x1) / 2;
    cy = (toggle.y0 + toggle.y1) / 2;

    /* same pre-grow interior point as the blit test above -- there, the
     * blit must preserve it (never dirty); here, the flag must force a full
     * redraw instead of a blit, so this point must come out dirty */
    interior_x = (before.x0 + outline_px + before.x1 - outline_px - icon) / 2;
    interior_y = (before.y0 + outline_px + titlebar_height + before.y1 - outline_px - icon) / 2;

    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* NB's toggle-size icon: grow */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_nb)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
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
      goto Failure; /* wuss_WINDOW_NO_RESIZE_BLIT must skip the blit path
                      * entirely, so even an interior pixel the blit would
                      * otherwise have preserved comes out dirty */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_nb);
  }

  printf("test: resizing a window only invalidates the grown/shrunk sliver\n");

  {
    static test_task_t tc_m2;
    wuss_task_t       *delegate_m2;
    box_t              box_m2, before2, after2, region;
    wuss_window_t     *win_m2;
    int                i, dirty_area, full_area, interior_x, interior_y, interior_dirty;

    tc_m2.redraw_count = 0;
    tc_m2.mouse_count  = 0;
    delegate_m2 = mk_task(wuss, test_handle, &tc_m2);
    if (delegate_m2 == NULL) goto Failure;

    box_m2.x0 = 0; box_m2.y0 = 0;
    box_m2.x1 = 40; box_m2.y1 = 40;
    rc = wuss_window_create(delegate_m2,
                            &box_m2,
                            "M2",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                            ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL,
                            wuss_NO_BACKDROP,
                            box_size(&box_m2),
                            SIZE2D(0, 0),
                            &win_m2);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the creation invalidation first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_m2, &before2);
    interior_x = before2.x0 + 2;  /* inside the untouched left edge */
    interior_y = before2.y0 + 25; /* below the titlebar, in plain content */

    rc = wuss_window_resize(win_m2, SIZE2D(80, 80)); /* grow */
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (interior_dirty)
      goto Failure; /* untouched top-left corner, unchanged by growing bottom-right */

    wuss_window_get_visible_bounds(win_m2, &after2);
    full_area  = (after2.x1 - after2.x0) * (after2.y1 - after2.y0);
    dirty_area = dirty_union_area(wuss, &after2);
    if (dirty_area < 0 || dirty_area >= full_area)
      goto Failure; /* must be less than a full redraw of the grown footprint */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    before2 = after2;

    rc = wuss_window_resize(win_m2, SIZE2D(40, 40)); /* shrink back */
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) == 0)
      goto Failure;

    interior_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, interior_x, interior_y))
        interior_dirty = 1;
    }
    if (interior_dirty)
      goto Failure; /* still untouched: the corner that remains after shrinking */

    full_area  = (before2.x1 - before2.x0) * (before2.y1 - before2.y0);
    dirty_area = dirty_union_area(wuss, &before2);
    if (dirty_area < 0 || dirty_area >= full_area)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_m2);
  }

  printf("test: resizing a wuss_WINDOW_NO_RESIZE_BLIT window redraws it fully\n");

  {
    static test_task_t tc_nb2;
    wuss_task_t       *delegate_nb2;
    box_t              box_nb2, before3, after3, region;
    wuss_window_t     *win_nb2;
    int                i, dirty_area, full_area;

    tc_nb2.redraw_count = 0;
    tc_nb2.mouse_count  = 0;
    delegate_nb2 = mk_task(wuss, test_handle, &tc_nb2);
    if (delegate_nb2 == NULL) goto Failure;

    box_nb2.x0 = 0; box_nb2.y0 = 0;
    box_nb2.x1 = 40; box_nb2.y1 = 40;
    rc = wuss_window_create(delegate_nb2, &box_nb2, "NB2",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_NO_BACKDROP,
                            box_size(&box_nb2),
                            SIZE2D(0, 0),
                            &win_nb2);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush the creation invalidation first */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_nb2, &before3);

    rc = wuss_window_resize(win_nb2, SIZE2D(80, 80)); /* grow */
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_nb2, &after3);
    full_area  = (after3.x1 - after3.x0) * (after3.y1 - after3.y0);
    dirty_area = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      dirty_area += (region.x1 - region.x0) * (region.y1 - region.y0);
    }
    if (dirty_area < full_area)
      goto Failure; /* NO_RESIZE_BLIT must fully redraw, not just the sliver */

    wuss_window_close(win_nb2);
  }

  printf("test: resizing a wuss_WINDOW_NO_RESIZE_BLIT window drops its cached "
         "furniture layout so the chrome redraws at the new width\n");

  {
    static test_task_t tc_fl;
    wuss_task_t       *delegate_fl;
    box_t              box_fl, after_fl;
    wuss_window_t     *win_fl;
    int                i, old_titlebar_x1, found;

    tc_fl.redraw_count = 0;
    tc_fl.mouse_count  = 0;
    delegate_fl = mk_task(wuss, test_handle, &tc_fl);
    if (delegate_fl == NULL) goto Failure;

    box_fl.x0 = 0; box_fl.y0 = 0;
    box_fl.x1 = 60; box_fl.y1 = 60;
    rc = wuss_window_create(delegate_fl, &box_fl, "FL",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_NO_BACKDROP,
                            box_size(&box_fl),
                            SIZE2D(0, 0),
                            &win_fl);
    if (rc != result_OK)
      goto Failure;

    /* build and cache the furniture layout at the creation width */
    wuss__furniture_layout_build(win_fl);
    if (!(win_fl->furniture_layout.flags & wuss_FURNITURE_LAYOUT__VALID))
      goto Failure;
    old_titlebar_x1 = win_fl->furniture_layout.titlebar.x1;

    rc = wuss_window_resize(win_fl, SIZE2D(140, 60)); /* grow the width */
    if (rc != result_OK)
      goto Failure;

    /* the resize must have invalidated the cache: nothing else here rebuilds
     * it, so a stale valid==1 means furniture would paint at the old width */
    if (win_fl->furniture_layout.flags & wuss_FURNITURE_LAYOUT__VALID)
      goto Failure;

    /* rebuilding now must track the new, wider window */
    wuss__furniture_layout_build(win_fl);
    wuss_window_get_visible_bounds(win_fl, &after_fl);
    if (win_fl->furniture_layout.titlebar.x1 <= old_titlebar_x1)
      goto Failure; /* titlebar still at the old width */

    /* the full-width divider rule must span to the new right edge too */
    found = 0;
    for (i = 0; i < win_fl->furniture_layout.npieces; i++)
    {
      const wuss__furniture_piece_t *p = &win_fl->furniture_layout.pieces[i];

      if (p->paint == wuss__FURNITURE_PAINT_OUTLINE &&
          p->rect.y1 == win_fl->furniture_layout.titlebar.y1 &&
          p->rect.x1 == win_fl->furniture_layout.titlebar.x1)
        found = 1;
    }
    if (!found)
      goto Failure;

    wuss_window_close(win_fl);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_resize_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_resize_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
