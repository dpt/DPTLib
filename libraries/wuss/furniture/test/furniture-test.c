/* furniture/test/furniture-test.c -- wuss window furniture tests */

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
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* Sweep every pixel of a window's visible box through wuss__furniture_hit_test
 * and check the tiling invariant: no pixel is wuss_FURNITURE_NONE, and a pixel
 * reports wuss_FURNITURE_CONTENT if and only if it lies inside the drawn
 * content box -- with one documented exception, the bare outline band of a
 * fully chromeless window (no titlebar, no scrollbars, no resize), where
 * content_may_leak is passed non-zero to allow CONTENT on 1px-frame pixels
 * outside the content box too. Returns 1 on pass, 0 on the first breach
 * (printing the offending pixel). */
static int furniture_hit_sweep(const wuss_window_t *window,
                               int                  content_may_leak)
{
  box_t visible, content;
  int   x, y, inside;

  wuss_window_get_visible_bounds((wuss_window_t *) window, &visible);
  wuss__content_box(window, &content);

  for (y = visible.y0; y < visible.y1; y++)
  {
    for (x = visible.x0; x < visible.x1; x++)
    {
      wuss_furniture_region_t region;

      region = wuss__furniture_hit_test(window, POINT(x, y));
      inside = box_contains_point(&content, x, y);

      if (region == wuss_FURNITURE_NONE)
      {
        printf("wuss_test: hit sweep: (%d,%d) is FURNITURE_NONE\n", x, y);
        return 0;
      }

      if (inside && region != wuss_FURNITURE_CONTENT)
      {
        printf("wuss_test: hit sweep: content pixel (%d,%d) reported %d\n",
               x, y, (int) region);
        return 0;
      }

      if (!inside && region == wuss_FURNITURE_CONTENT && !content_may_leak)
      {
        printf("wuss_test: hit sweep: chrome pixel (%d,%d) reported CONTENT\n",
               x, y);
        return 0;
      }
    }
  }

  return 1;
}

/* ----------------------------------------------------------------------- */

result_t wuss_furniture_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: furniture hit boxes tile the visible box with no leaks\n");

  {
    static test_task_t tc_hs;
    wuss_task_t       *delegate_hs;
    box_t              box_hs, visible, content;
    box_t              back, close, toggle, resize;
    box_t              vup, vdown, vwell, hleft, hright, hwell;
    wuss_window_t     *win_hs;
    int                midx, midy;

    tc_hs.redraw_count = 0;
    tc_hs.mouse_count  = 0;
    delegate_hs = mk_task(wuss, test_handle, &tc_hs);
    if (delegate_hs == NULL) goto Failure;

    /* 120x80 content: wide enough that BACK, CLOSE and TOGGLE_SIZE do not
     * overlap in the titlebar. */
    box_hs.x0 = 5; box_hs.y0 = 5;
    box_hs.x1 = 125; box_hs.y1 = 85;
    rc = wuss_window_create(delegate_hs, &box_hs, "HS", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_hs);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_hs, &visible);
    wuss_window_get_content_bounds(win_hs, &content);
    wuss__back_box(win_hs, &back);
    wuss__close_box(win_hs, &close);
    wuss__toggle_box(win_hs, &toggle);
    wuss__resize_box(win_hs, &resize);
    wuss__vscroll_up_box(win_hs, &vup);
    wuss__vscroll_down_box(win_hs, &vdown);
    wuss__vscroll_well_box(win_hs, &vwell);
    wuss__hscroll_left_box(win_hs, &hleft);
    wuss__hscroll_right_box(win_hs, &hright);
    wuss__hscroll_well_box(win_hs, &hwell);

    midx = (content.x0 + content.x1) / 2;
    midy = (content.y0 + content.y1) / 2;

    /* outline band, each edge */
    if (wuss__furniture_hit_test(win_hs, POINT(midx, visible.y0)) != wuss_FURNITURE_TITLE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x0, midy)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure; /* left outline column, level with the content: nearest chrome is the hscroll strip */
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x1 - 1, midy)) != wuss_FURNITURE_VSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(midx, visible.y1 - 1)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure;

    /* the four window corners belong to the adjacent furniture */
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x0, visible.y0)) != wuss_FURNITURE_BACK)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x1 - 1, visible.y0)) != wuss_FURNITURE_TOGGLE_SIZE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x1 - 1, visible.y1 - 1)) != wuss_FURNITURE_RESIZE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(visible.x0, visible.y1 - 1)) != wuss_FURNITURE_HSCROLL_LEFT)
      goto Failure;

    /* icon centres */
    if (wuss__furniture_hit_test(win_hs, POINT((back.x0 + back.x1) / 2, (back.y0 + back.y1) / 2)) != wuss_FURNITURE_BACK)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((close.x0 + close.x1) / 2, (close.y0 + close.y1) / 2)) != wuss_FURNITURE_CLOSE)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((toggle.x0 + toggle.x1) / 2, (toggle.y0 + toggle.y1) / 2)) != wuss_FURNITURE_TOGGLE_SIZE)
      goto Failure;

    /* scroll-part centres */
    if (wuss__furniture_hit_test(win_hs, POINT((vup.x0 + vup.x1) / 2, (vup.y0 + vup.y1) / 2)) != wuss_FURNITURE_VSCROLL_UP)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((vdown.x0 + vdown.x1) / 2, (vdown.y0 + vdown.y1) / 2)) != wuss_FURNITURE_VSCROLL_DOWN)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((vwell.x0 + vwell.x1) / 2, (vwell.y0 + vwell.y1) / 2)) != wuss_FURNITURE_VSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((hleft.x0 + hleft.x1) / 2, (hleft.y0 + hleft.y1) / 2)) != wuss_FURNITURE_HSCROLL_LEFT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((hright.x0 + hright.x1) / 2, (hright.y0 + hright.y1) / 2)) != wuss_FURNITURE_HSCROLL_RIGHT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT((hwell.x0 + hwell.x1) / 2, (hwell.y0 + hwell.y1) / 2)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure;

    /* divider seam between the content and the scroll strips */
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1, midy)) != wuss_FURNITURE_VSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(midx, content.y1)) != wuss_FURNITURE_HSCROLL_WELL)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1, content.y1 - 1)) == wuss_FURNITURE_CONTENT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1 - 1, content.y1)) == wuss_FURNITURE_CONTENT)
      goto Failure;

    /* genuine content */
    if (wuss__furniture_hit_test(win_hs, POINT(content.x0 + 5, content.y0 + 5)) != wuss_FURNITURE_CONTENT)
      goto Failure;
    if (wuss__furniture_hit_test(win_hs, POINT(content.x1 - 1, content.y1 - 1)) != wuss_FURNITURE_CONTENT)
      goto Failure;

    if (!furniture_hit_sweep(win_hs, 0))
      goto Failure;

    wuss_window_close(win_hs);
  }

  printf("test: a held scroll arrow auto-repeats after a delay, pauses off "
         "the arrow and stops on release\n");

  {
    static test_task_t tc_sr;
    wuss_task_t       *delegate_sr;
    box_t              box_sr, vdown;
    wuss_window_t     *win_sr;
    point_t            at, scroll;
    int                i;

    delegate_sr = mk_task(wuss, test_handle, &tc_sr);
    if (delegate_sr == NULL) goto Failure;

    box_sr.x0 = 5; box_sr.y0 = 5;
    box_sr.x1 = 125; box_sr.y1 = 85;
    rc = wuss_window_create(delegate_sr, &box_sr, "SR", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_sr);
    if (rc != result_OK) goto Failure;

    wuss__vscroll_down_box(win_sr, &vdown);
    at = POINT((vdown.x0 + vdown.x1) / 2, (vdown.y0 + vdown.y1) / 2);

    /* the press itself steps once */
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != WUSS_SCROLL_STEP) goto Failure;

    /* nothing more until the delay runs out, then one step */
    for (i = 0; i < WUSS_SCROLL_REPEAT_DELAY - 1; i++)
      wuss_idle(wuss);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != WUSS_SCROLL_STEP) goto Failure;
    wuss_idle(wuss);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != 2 * WUSS_SCROLL_STEP) goto Failure;

    /* then one step per interval */
    for (i = 0; i < WUSS_SCROLL_REPEAT_INTERVAL; i++)
      wuss_idle(wuss);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != 3 * WUSS_SCROLL_STEP) goto Failure;

    /* off the arrow (still held): paused */
    wuss_mouse_move(wuss, POINT(20, 20), NULL);
    for (i = 0; i < 4 * WUSS_SCROLL_REPEAT_INTERVAL; i++)
      wuss_idle(wuss);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != 3 * WUSS_SCROLL_STEP) goto Failure;

    /* back on: resumes */
    wuss_mouse_move(wuss, at, NULL);
    for (i = 0; i < WUSS_SCROLL_REPEAT_INTERVAL; i++)
      wuss_idle(wuss);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != 4 * WUSS_SCROLL_STEP) goto Failure;

    /* released: stops */
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    for (i = 0; i < 2 * WUSS_SCROLL_REPEAT_DELAY; i++)
      wuss_idle(wuss);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != 4 * WUSS_SCROLL_STEP) goto Failure;

    /* ADJUST repeats the other way */
    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, NULL);
    for (i = 0; i < WUSS_SCROLL_REPEAT_DELAY; i++)
      wuss_idle(wuss);
    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, NULL);
    wuss_window_get_scroll(win_sr, &scroll);
    if (scroll.y != 2 * WUSS_SCROLL_STEP) goto Failure;

    wuss_window_close(win_sr);
  }

  printf("test: Select-dragging a sausage scrolls its own axis; "
         "Adjust-dragging scrolls both\n");

  {
    static test_task_t tc_sd;
    wuss_task_t       *delegate_sd;
    box_t              box_sd, sausage;
    wuss_window_t     *win_sd;
    point_t            at, scroll;
    int                sy;

    delegate_sd = mk_task(wuss, test_handle, &tc_sd);
    if (delegate_sd == NULL) goto Failure;

    box_sd.x0 = 5; box_sd.y0 = 5;
    box_sd.x1 = 125; box_sd.y1 = 85;
    rc = wuss_window_create(delegate_sd, &box_sd, "SD", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_sd);
    if (rc != result_OK) goto Failure;

    wuss__vscroll_sausage_box(win_sd, &sausage);
    at = POINT((sausage.x0 + sausage.x1) / 2, (sausage.y0 + sausage.y1) / 2);

    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_move(wuss, POINT(at.x + 10, at.y + 10), NULL);
    wuss_mouse_click(wuss, POINT(at.x + 10, at.y + 10), wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    wuss_window_get_scroll(win_sd, &scroll);
    if (scroll.y <= 0 || scroll.x != 0) goto Failure;

    sy = scroll.y;

    wuss__vscroll_sausage_box(win_sd, &sausage);
    at = POINT((sausage.x0 + sausage.x1) / 2, (sausage.y0 + sausage.y1) / 2);

    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_move(wuss, POINT(at.x + 10, at.y + 10), NULL);
    wuss_mouse_click(wuss, POINT(at.x + 10, at.y + 10), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, NULL);
    wuss_window_get_scroll(win_sd, &scroll);
    if (scroll.y <= sy || scroll.x <= 0) goto Failure;

    wuss_window_close(win_sd);
  }

  printf("test: furniture hit tiling holds for every furniture-flag combo\n");

  {
    static const wuss_window_flags_t combos[] =
    {
      wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_OUTLINE,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_RESIZE,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL & ~wuss_WINDOW_HSCROLL,
      /* one scrollbar + resize: carve is 0 on the stripless axis, so the
       * icon's near edge is the only thing sizing its hit box */
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_HSCROLL,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_TOGGLE_SIZE,
      wuss_WINDOW_DEFAULT & ~wuss_WINDOW_CLOSE,
      wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_TITLEBAR
    };

    static test_task_t tc_cb;
    wuss_task_t       *delegate_cb;
    box_t              box_cb, visible;
    wuss_window_t     *win_cb;
    unsigned int       i;

    for (i = 0; i < sizeof combos / sizeof combos[0]; i++)
    {
      int leak;

      tc_cb.redraw_count = 0;
      tc_cb.mouse_count  = 0;
      delegate_cb = mk_task(wuss, test_handle, &tc_cb);
      if (delegate_cb == NULL) goto Failure;

      box_cb.x0 = 5; box_cb.y0 = 5;
      box_cb.x1 = 125; box_cb.y1 = 85;
      rc = wuss_window_create(delegate_cb, &box_cb, "CB", combos[i],
                              wuss_NO_BACKDROP,
                              SIZE2D(400, 400), SIZE2D(0, 0), &win_cb);
      if (rc != result_OK)
        goto Failure;

      /* No titlebar still keeps a 1px top frame with nothing behind it: a
       * click there is the documented CONTENT exception. */
      leak = (combos[i] & wuss_WINDOW_NO_TITLEBAR) &&
             !(combos[i] & wuss_WINDOW_NO_OUTLINE);

      if (!furniture_hit_sweep(win_cb, leak))
      {
        printf("wuss_test: combo index %u (flags 0x%X) failed the hit sweep\n",
               i, (unsigned int) combos[i]);
        goto Failure;
      }

      /* Where the window keeps its resize icon, the icon's own drawn centre
       * must hit RESIZE. The sweep alone misses a collapsed hit box: those
       * pixels just fall to a neighbouring region, still leak-free. */
      if (combos[i] & wuss_WINDOW_RESIZE)
      {
        box_t resize;

        wuss__resize_box(win_cb, &resize);
        if (wuss__furniture_hit_test(win_cb,
              POINT((resize.x0 + resize.x1) / 2,
                    (resize.y0 + resize.y1) / 2)) != wuss_FURNITURE_RESIZE)
        {
          printf("wuss_test: combo index %u (flags 0x%X): resize icon centre "
                 "does not hit RESIZE\n", i, (unsigned int) combos[i]);
          goto Failure;
        }
      }

      if (!(combos[i] & wuss_WINDOW_BACK) &&
          wuss__furniture_hit_test(win_cb, POINT(box_cb.x0, box_cb.y0)) != wuss_FURNITURE_CLOSE)
      {
        wuss_window_get_visible_bounds(win_cb, &visible);
        if (wuss__furniture_hit_test(win_cb, POINT(visible.x0, visible.y0)) != wuss_FURNITURE_CLOSE)
          goto Failure;
      }

      wuss_window_close(win_cb);
    }
  }

  printf("test: a titlebar carries a full-width divider rule at its foot\n");

  {
    static test_task_t tc_dv;
    wuss_task_t       *delegate_dv;
    box_t              box_dv;
    wuss_window_t     *win_dv;
    int                i, found;

    tc_dv.redraw_count = 0;
    tc_dv.mouse_count  = 0;
    delegate_dv = mk_task(wuss, test_handle, &tc_dv);
    if (delegate_dv == NULL) goto Failure;

    box_dv.x0 = 5; box_dv.y0 = 5;
    box_dv.x1 = 125; box_dv.y1 = 85;
    rc = wuss_window_create(delegate_dv, &box_dv, "DV", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_dv);
    if (rc != result_OK)
      goto Failure;

    wuss__furniture_layout_build(win_dv);

    /* an OUTLINE piece exactly one pixel tall, sitting on the titlebar's
     * bottom edge and spanning its full width */
    found = 0;
    for (i = 0; i < win_dv->furniture_layout.npieces; i++)
    {
      const wuss__furniture_piece_t *p = &win_dv->furniture_layout.pieces[i];

      if (p->paint == wuss__FURNITURE_PAINT_OUTLINE &&
          p->rect.y1 == win_dv->furniture_layout.titlebar.y1 &&
          p->rect.y1 - p->rect.y0 == WUSS_DIVIDER_PX &&
          p->rect.x0 == win_dv->furniture_layout.titlebar.x0 &&
          p->rect.x1 == win_dv->furniture_layout.titlebar.x1)
        found = 1;
    }
    if (!found)
      goto Failure;

    wuss_window_close(win_dv);
  }

  printf("test: every furniture element has a hit box and a press handler\n");
  {
    const wuss__furniture_element_t *element;
    int                              i;

    for (i = 0; i < wuss__furniture_nelements; i++)
    {
      element = &wuss__furniture_elements[i];

      if (element->hit_box == NULL || element->press == NULL)
        goto Failure;

      if (wuss__furniture_element(element->region) != element)
        goto Failure;
    }
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_furniture_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_furniture_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
