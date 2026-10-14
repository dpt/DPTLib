/* wuss/test/wuss-test.c -- wuss - minimal window manager */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/filetype.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/message.h"
#ifdef WUSS_MENUS
#include "wuss/menu.h"
#include "wuss/menu-desc.h"
#endif
#ifdef WUSS_COMPONENTS
#include "wuss/component/dialogue.h"
#include "wuss/component/fontmenu.h"
#include "wuss/component/colourmenu.h"
#include "wuss/component/saveas.h"
#include "wuss/gadget/colourset.h"
#endif
#ifdef WUSS_GADGETS
#include "wuss/gadget/stringset.h"
#include "wuss/gadget/gridview.h"
#endif

/* white-box: the furniture hit-test sweep calls wuss__furniture_hit_test and
 * the box helpers from impl.h / furniture.h directly */
#if defined(WUSS_ICONS)
#include "../core/impl.h"
#include "wuss/icon-spec.h"
#include "../icon.h"
#endif
#if defined(WUSS_ICONBAR)
#include "../iconbar.h"
#endif

#include "test/all-tests.h"
#include "test/tmpdir.h"
#include "harness.h"

/* ----------------------------------------------------------------------- */

/* The SDL interactive driver that used to live here (spawn_* callbacks, the
 * launcher table and the run_wuss loop) is now the standalone `wuss` app
 * in apps/wuss/main.c. This file is the Wuss unit test only. */

/* ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------- */

/* ----------------------------------------------------------------------- */

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

result_t wuss_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  test_task_t    tc_a, tc_b, tc_c, tc_d;
  wuss_task_t   *delegate_a, *delegate_b, *delegate_c, *delegate_d;
  box_t          box_a, box_b, box_c, box_d;
  wuss_window_t *win_a, *win_b, *win_c, *win_d;
  wuss_window_t *hit;
  box_t          visible, content;
  int            before_a, before_b;
  int            width, height;

#ifndef WUSS_GADGETS
  NOT_USED(resources); /* only the gadget and component tests read it */
#endif

  rowbytes = 200 * 4;
  pixels = malloc(rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes, NULL, pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 0; /* zero-height content is invalid regardless of furniture */
  rc = wuss_window_create(mk_task(wuss, NULL, NULL),
                          &box_a,
                          "toosmall",
                          wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP,
                          box_size(&box_a),
                          SIZE2D(0, 0),
                          &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  memset(&tc_a, 0, sizeof(tc_a));
  delegate_a = mk_task(wuss, test_handle, &tc_a);
  if (delegate_a == NULL) goto Failure;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(delegate_a,
                          &box_a,
                          "A",
                          wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                          ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                          ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                          wuss_NO_BACKDROP,
                          box_size(&box_a),
                          SIZE2D(0, 0),
                          &win_a);
  if (rc != result_OK)
    goto Failure;

  memset(&tc_b, 0, sizeof(tc_b));
  delegate_b = mk_task(wuss, test_handle, &tc_b);
  if (delegate_b == NULL) goto Failure;

  box_b.x0 = 50;
  box_b.y0 = 50;
  box_b.x1 = 150;
  box_b.y1 = 150;
  rc = wuss_window_create(delegate_b,
                          &box_b,
                          "B",
                          wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                          ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                          ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                          wuss_NO_BACKDROP,
                          box_size(&box_b),
                          SIZE2D(0, 0),
                          &win_b);
  if (rc != result_OK)
    goto Failure;

  printf("test: redraw\n");

  rc = wuss_redraw(wuss);
  if (rc != result_OK)
    goto Failure;
  /* A's visible footprint is L-shaped (B covers its bottom-right corner),
   * so it's redrawn as two non-overlapping pieces; B is unoccluded, one */
  if (tc_a.redraw_count != 2 || tc_b.redraw_count != 1)
    goto Failure;

  printf("test: invalidating an area of A fully covered by topmost B is discarded\n");

  {
    box_t local;

    local.x0 = 60;
    local.y0 = 60;
    local.x1 = 90;
    local.y1 = 90; /* well within B's (50,50)-(150,150)+furniture visible footprint */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;

    local.x0 = 0;
    local.y0 = 0;
    local.x1 = 20;
    local.y1 = 20; /* outside B's footprint entirely: nothing to clip away */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
  }

  printf("test: a piece straddling B's edge redraws A but not B\n");

  {
    box_t local;

    before_a = tc_a.redraw_count;
    before_b = tc_b.redraw_count;

    local.x0 = 30;
    local.y0 = 60;
    local.x1 = 70;
    local.y1 = 90; /* straddles B's left edge (x=49): only x:30-49 survives clipping */
    wuss_window_invalidate(win_a, &local);
    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_a.redraw_count != before_a + 1)
      goto Failure;
    if (tc_b.redraw_count != before_b)
      goto Failure; /* B not touched by the surviving piece: must not be redrawn */
  }

  printf("test: z-order hit test and local coordinate translation (B on top)\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 25)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: close icon runs wuss_window_try_close; a PRE_CLOSE veto keeps the window\n");

  tc_a.pre_close_count = 0;
  tc_a.close_count     = 0;
  tc_a.veto_pre_close  = 1;
  rc = wuss_mouse_click(wuss, POINT(6, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_BAD_ARG)
    goto Failure; /* the veto's non-OK return propagates out of the click */
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit); /* if the close click had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  if (tc_a.pre_close_count != 1)
    goto Failure; /* the close icon asked the task first */
  if (tc_a.close_count != 0)
    goto Failure; /* vetoed: no CLOSE, window not torn down */

  wuss_window_get_visible_bounds(win_a, &visible); /* still alive */
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started by the close click */

  tc_a.veto_pre_close = 0;

  printf("test: ADJUST on the close icon sends CLOSE_ADJUST, with the held modifiers, and keeps the window\n");

  tc_a.pre_close_count = 0;
  wuss_set_modifiers(wuss, wuss_KEY_MOD_SHIFT);
  rc = wuss_mouse_click(wuss, POINT(6, 11), wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit); /* if the press had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  wuss_set_modifiers(wuss, wuss_KEY_MOD_NONE);

  if (tc_a.close_adjust_count != 1 || tc_a.pre_close_count != 0)
    goto Failure; /* told the task, did not try to close */
  if (tc_a.close_adjust_mods != wuss_KEY_MOD_SHIFT)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible); /* still alive */
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started */

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click does not change z-order\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* B's content, only within B */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: two same-button, same-window DOWNs within the double-click "
        "window carry wuss_BUTTON_DOUBLE on the second; a third does not "
        "chain into another double\n");

  wuss_set_time(wuss, 1000);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (tc_b.last_button & wuss_BUTTON_DOUBLE)
    goto Failure; /* first press of the pair: not a double */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 1100); /* 100ms later, well within the 400ms default */
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (!(tc_b.last_button & wuss_BUTTON_DOUBLE))
    goto Failure; /* same window, same button, within time and distance */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 1150);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (tc_b.last_button & wuss_BUTTON_DOUBLE)
    goto Failure; /* a third press starts a fresh pair, does not chain */

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: a same-window DOWN outside double_click_ms does not carry "
        "wuss_BUTTON_DOUBLE\n");

  wuss_set_time(wuss, 2000);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 2500); /* 500ms later, past the 400ms default */
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (tc_b.last_button & wuss_BUTTON_DOUBLE)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: a DOWN on a different window resets the double-click "
        "pair even within the time window\n");

  wuss_set_time(wuss, 3000);
  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* B */
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(120, 120), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  wuss_set_time(wuss, 3100);
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A, different window */
  if (rc != result_OK)
    goto Failure;
  if (tc_a.last_button & wuss_BUTTON_DOUBLE)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A still topmost: B's content click above didn't bring it to front */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1 || tc_a.last_x != 74 || tc_a.last_y != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's titlebar, A already topmost, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 0)
    goto Failure;

  printf("test: drag-move updates visible bounds and invalidates the affected region\n");

  rc = wuss_redraw_dirty(wuss); /* flush the click-to-front's leftover dirty region first */
  if (rc != result_OK)
    goto Failure;

  before_a = tc_a.redraw_count;
  before_b = tc_b.redraw_count;
  rc = wuss_mouse_move(wuss, POINT(31, 36), &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* invalidated, not yet redrawn */

  if (wuss_get_dirty_count(wuss) == 0)
    goto Failure;

  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != before_a || tc_b.redraw_count != before_b)
    goto Failure; /* blitted, not redrawn: A's own pixels moved without a task
                    * callback, and the vacated sliver behind its old position
                    * exposes only background, not B */

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
    goto Failure;

  printf("test: mouse-up ends the drag\n");

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  printf("test: Adjust-drag moves a window without bringing it to front\n");

  rc = wuss_mouse_click(wuss, POINT(140, 35), wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* B's titlebar, clear of A */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(145, 60), &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  wuss_window_get_visible_bounds(win_b, &visible);
  if (visible.x0 != 54 || visible.y0 != 54)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(145, 60), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within both A and B; A still topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(200, 200), &hit); /* off all windows, drag must have ended */
  if (rc != result_OK)
    goto Failure;
  if (hit != NULL)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 25)
    goto Failure;

  if (tc_a.open_count != 1)
    goto Failure; /* wuss_EVENT_OPEN sent once for the drag-move above */

  printf("test: window_resize valid and too-small cases\n");

  rc = wuss_window_resize(win_a, SIZE2D(50, 0)); /* zero-height content is invalid */
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;
  if (tc_a.open_count != 1)
    goto Failure; /* rejected resize: no wuss_EVENT_OPEN */

  rc = wuss_window_resize(win_a, SIZE2D(50, 50));
  if (rc != result_OK)
    goto Failure;
  if (tc_a.open_count != 2)
    goto Failure; /* wuss_EVENT_OPEN sent for the successful resize */

  /* content ends up exactly the requested size... */
  wuss_window_get_content_bounds(win_a, &content);
  width  = content.x1 - content.x0;
  height = content.y1 - content.y0;
  if (width != 50 || height != 50)
    goto Failure;

  /* ...with the titlebar/outline furniture added on top of that */
  wuss_window_get_visible_bounds(win_a, &visible);
  width  = visible.x1 - visible.x0;
  height = visible.y1 - visible.y0;
  if (width != 52 || height != 72)
    goto Failure;

  printf("test: window_resize caps the size at the screen edge\n");

  /* screen is 200x200; win_a's visible box sits at (0,25) with a 1px
   * outline all round and a 20px titlebar. asking for a 500x500 content
   * area is capped so the visible box ends at the screen edge (198 wide,
   * 153 tall of content). */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 198 || content.y1 - content.y0 != 153)
    goto Failure;
  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x1 != 200 || visible.y1 != 200)
    goto Failure;

  /* a smaller request is honoured verbatim */
  rc = wuss_window_resize(win_a, SIZE2D(60, 40));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 60 || content.y1 - content.y0 != 40)
    goto Failure;

  /* restore for the tests that follow */
  rc = wuss_window_resize(win_a, SIZE2D(50, 50));
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create can never make a window bigger than the "
         "screen\n");

  {
    box_t          box_big;
    wuss_window_t *win_big;

    /* content box asks for 10,10..400,400; the on-screen nudge pulls the
     * visible top-left (10-1 outline) back to (0,0), then the clamp caps
     * the content at 200 - 2*1 - 20(titlebar) tall, 200 - 2*1 wide. */
    box_big.x0 = 10; box_big.y0 = 10; box_big.x1 = 400; box_big.y1 = 400;
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_big, "BIG",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_big);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_big, &visible);
    if (visible.x0 != 0 || visible.y0 != 0 ||
        visible.x1 != 200 || visible.y1 != 200)
      goto Failure;
    wuss_window_close(win_big);
  }

  printf("test: title-less, no-outline window has no furniture, so visible == content\n");

  tc_d.redraw_count = 0;
  tc_d.mouse_count  = 0;
  delegate_d = mk_task(wuss, test_handle, &tc_d);
  if (delegate_d == NULL) goto Failure;

  box_d.x0 = 0;  box_d.y0 = 160;
  box_d.x1 = 30; box_d.y1 = 175; /* shorter than the 20px titlebar_height, still valid: no titlebar to fit */
  rc = wuss_window_create(delegate_d,
                          &box_d,
                          "ignored",
                          /* fully chromeless: no titlebar, outline, scrollbars or resize */
                          wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE,
                          wuss_NO_BACKDROP,
                          box_size(&box_d),
                          SIZE2D(0, 0),
                          &win_d);
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_d, &visible);
  if (visible.x0 != box_d.x0 || visible.y0 != box_d.y0 ||
      visible.x1 != box_d.x1 || visible.y1 != box_d.y1)
    goto Failure;

  printf("test: click within a title-less window's top edge is delivered as content, not a drag\n");

  rc = wuss_mouse_click(wuss, POINT(5, 165), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_d)
    goto Failure;
  if (tc_d.mouse_count != 1 || tc_d.last_action != wuss_MOUSE_DOWN || tc_d.last_x != 5 || tc_d.last_y != 5)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(5, 165), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: content click on a title-less window does not change z-order\n");

  {
    static test_task_t tc_e, tc_f;
    wuss_task_t       *delegate_e, *delegate_f;
    box_t              box_e, box_f;
    wuss_window_t     *win_e, *win_f;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e = mk_task(wuss, test_handle, &tc_e);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 100; box_e.y0 = 0;
    box_e.x1 = 150; box_e.y1 = 50;
    rc = wuss_window_create(delegate_e,
                            &box_e,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR,
                            wuss_NO_BACKDROP,
                            box_size(&box_e),
                            SIZE2D(0, 0),
                            &win_e);
    if (rc != result_OK)
      goto Failure;

    tc_f.redraw_count = 0;
    tc_f.mouse_count  = 0;
    delegate_f = mk_task(wuss, test_handle, &tc_f);
    if (delegate_f == NULL) goto Failure;

    box_f.x0 = 130; box_f.y0 = 20;
    box_f.x1 = 180; box_f.y1 = 70;
    rc = wuss_window_create(delegate_f,
                            &box_f,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR,
                            wuss_NO_BACKDROP,
                            box_size(&box_f),
                            SIZE2D(0, 0),
                            &win_f);
    if (rc != result_OK)
      goto Failure;

    /* F was created after E, so F is topmost; clicking E's exposed content
     * (outside the overlap) is delivered to E but must not raise it */
    rc = wuss_mouse_click(wuss, POINT(110, 10), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* within E only */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(110, 10), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(135, 25), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: F still on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_f)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(135, 25), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_e);
    wuss_window_close(win_f);
  }

  printf("test: wuss_window_set_background\n");

  rc = wuss_window_set_background(win_d, wuss_BACKDROP_COLOUR(100));
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  rc = wuss_window_set_background(win_d, wuss_BACKDROP_COLOUR(1));
  if (rc != result_OK)
    goto Failure;

  wuss_window_close(win_d);

  printf("test: Adjust-click on a window's back icon brings it to front\n");

  {
    static test_task_t tc_g, tc_h;
    wuss_task_t       *delegate_g, *delegate_h;
    box_t              box_g, box_h;
    wuss_window_t     *win_g, *win_h;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h = mk_task(wuss, test_handle, &tc_h);
    if (delegate_h == NULL) goto Failure;

    box_h.x0 = 130; box_h.y0 = 50;
    box_h.x1 = 190; box_h.y1 = 100;
    rc = wuss_window_create(delegate_h,
                            &box_h,
                            "H",
                            wuss_WINDOW_DEFAULT,
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

    box_g.x0 = 110; box_g.y0 = 30;
    box_g.x1 = 160; box_g.y1 = 80;
    rc = wuss_window_create(delegate_g,
                            &box_g,
                            "G",
                            wuss_WINDOW_DEFAULT & ~wuss_WINDOW_VSCROLL &
                            ~wuss_WINDOW_HSCROLL,
                            wuss_NO_BACKDROP,
                            box_size(&box_g),
                            SIZE2D(0, 0),
                            &win_g);
    if (rc != result_OK)
      goto Failure;

    /* G is topmost here, overlapping H; G's own back icon (top-left
     * corner) never falls under H, so it stays clickable either way */
    wuss_window_get_visible_bounds(win_g, &visible);

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap of G and H */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: H now on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_h) /* G was sent to back */
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, &hit); /* Adjust-click G's back icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(visible.x0 + 5, visible.y0 + 5), wuss_BUTTON_ADJUST, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* overlap: G back on top */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g) /* Adjust-click on the back icon brought G back to front */
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(145, 65), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    printf("test: drag-resize stops at doc_width/doc_height, not just WUSS_MIN_CONTENT\n");

    wuss_window_get_content_bounds(win_g, &content); /* G's doc_width/doc_height are 50x50, same as its initial content size */

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_g)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(content.x0 + 50, content.y0 + 50), &hit); /* drag to exactly the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(content.x0 + 50, content.y0 + 50), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    width  = content.x1 - content.x0;
    height = content.y1 - content.y0;

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* G's resize icon again */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(content.x0 + 500, content.y0 + 500), &hit); /* drag far past the doc extent */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(content.x0 + 500, content.y0 + 500), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_g, &content);
    if (content.x1 - content.x0 != width || content.y1 - content.y0 != height)
      goto Failure; /* clamped to the same size as dragging to exactly the doc extent: not left to grow past it */

    wuss_window_close(win_g);
    wuss_window_close(win_h);
  }

  printf("test: drag-resize stops at min_doc, not just WUSS_MIN_CONTENT\n");

  {
    static test_task_t tc_m;
    wuss_task_t       *delegate_m;
    box_t              box_m, content, visible;
    wuss_window_t     *win_m;

    tc_m.redraw_count    = 0;
    tc_m.mouse_count     = 0;
    delegate_m = mk_task(wuss, test_handle, &tc_m);
    if (delegate_m == NULL) goto Failure;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 210; box_m.y1 = 210; /* 200x200 content, floored at 80x60 */
    rc = wuss_window_create(delegate_m, &box_m, "M", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(200, 200), SIZE2D(80, 60),
                            &win_m);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_m, &visible);
    wuss_window_get_content_bounds(win_m, &content);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* M's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_m)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(content.x0 + 5, content.y0 + 5), &hit); /* drag far inside min_doc */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(content.x0 + 5, content.y0 + 5), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_m, &content);
    if (content.x1 - content.x0 != 80 || content.y1 - content.y0 != 60)
      goto Failure; /* floored at min_doc, not squeezed down to WUSS_MIN_CONTENT */

    wuss_window_close(win_m);
  }

  printf("test: drag-resize stops where the window's furniture still fits\n");

  {
    static test_task_t tc_f;
    wuss_task_t       *delegate_f;
    box_t              box_f, content, visible;
    box_t              close, toggle, vup, vdown, hleft, hright;
    wuss_window_t     *win_f;

    delegate_f = mk_task(wuss, test_handle, &tc_f);
    if (delegate_f == NULL) goto Failure;

    /* a doc smaller than the furniture: only the furniture floor holds it */
    box_f.x0 = 10; box_f.y0 = 10;
    box_f.x1 = 110; box_f.y1 = 110;
    rc = wuss_window_create(delegate_f, &box_f, "F", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1, 1), SIZE2D(0, 0), &win_f);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_f, &visible);
    wuss_window_get_content_bounds(win_f, &content);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* F's resize icon */
    if (rc != result_OK || hit != win_f)
      goto Failure;
    rc = wuss_mouse_move(wuss, POINT(content.x0, content.y0), &hit); /* try to squash it flat */
    if (rc != result_OK)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(content.x0, content.y0), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss__close_box(win_f, &close);
    wuss__toggle_box(win_f, &toggle);
    wuss__vscroll_up_box(win_f, &vup);
    wuss__vscroll_down_box(win_f, &vdown);
    wuss__hscroll_left_box(win_f, &hleft);
    wuss__hscroll_right_box(win_f, &hright);

    if (toggle.x0 < close.x1 + WUSS_BUTTON_INSET)
      goto Failure; /* titlebar icons overlap */
    if (vdown.y0 < vup.y1 || hright.x0 < hleft.x1)
      goto Failure; /* scroll arrows overlap */

    wuss_window_close(win_f);
  }

  printf("test: drag-resize never grows a window's total size past the screen's, on either axis\n");

  {
    static test_task_t tc_s;
    wuss_task_t       *delegate_s;
    box_t              box_s, content, visible;
    wuss_window_t     *win_s;

    tc_s.redraw_count = 0;
    tc_s.mouse_count  = 0;
    delegate_s = mk_task(wuss, test_handle, &tc_s);
    if (delegate_s == NULL) goto Failure;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 30; box_s.y1 = 30; /* 20x20 content; doc is 1000x1000, far bigger than the 200x200 screen */
    rc = wuss_window_create(delegate_s, &box_s, "S", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_s, &visible);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* S's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(1000, 1000), &hit); /* drag far past the 200x200 screen */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(1000, 1000), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_s, &visible);
    if (visible.x1 - visible.x0 > 200 || visible.y1 - visible.y0 > 200)
      goto Failure; /* total footprint capped at the screen size, wherever the window sits */

    wuss_window_close(win_s);
  }

  {
    /* Same cap, but for a window moved partway off the top-left of the
     * screen: the mouse pointer itself is clamped to the screen (see
     * wuss_mouse_move), so a positive content.x0 can never see a resize
     * drag reach far enough to overhang -- the gap only shows up once
     * content.x0 is negative, letting the drag's on-screen pointer range
     * translate into a width bigger than the screen itself. */
    static test_task_t tc_e;
    wuss_task_t       *delegate_e;
    box_t              box_e, visible;
    wuss_window_t     *win_e;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e = mk_task(wuss, test_handle, &tc_e);
    if (delegate_e == NULL) goto Failure;

    box_e.x0 = 10; box_e.y0 = 10;
    box_e.x1 = 30; box_e.y1 = 30; /* 20x20 content; doc is 1000x1000, far bigger than the 200x200 screen */
    rc = wuss_window_create(delegate_e, &box_e, "E", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(1000, 1000), SIZE2D(0, 0), &win_e);
    if (rc != result_OK)
      goto Failure;

    wuss_window_move(win_e, POINT(-100, -100)); /* content top-left off-screen, top-left corner */

    wuss_window_get_visible_bounds(win_e, &visible);

    rc = wuss_mouse_click(wuss, POINT(visible.x1 - 3, visible.y1 - 3), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* E's resize icon */
    if (rc != result_OK)
      goto Failure;
    if (hit != win_e)
      goto Failure;

    rc = wuss_mouse_move(wuss, POINT(1000, 1000), &hit); /* drag to the far corner; the pointer itself clamps to (199,199) */
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_click(wuss, POINT(1000, 1000), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_e, &visible);
    if (visible.x1 - visible.x0 > 200 || visible.y1 - visible.y0 > 200)
      goto Failure; /* total footprint still capped at the screen size, even with a negative x0/y0 */

    wuss_window_close(win_e);
  }

  printf("test: destroy mid-drag then move doesn't crash\n");

  tc_c.redraw_count = 0;
  tc_c.mouse_count  = 0;
  delegate_c = mk_task(wuss, test_handle, &tc_c);
  if (delegate_c == NULL) goto Failure;

  box_c.x0 = 0;
  box_c.y0 = 0;
  box_c.x1 = 40;
  box_c.y1 = 40;
  rc = wuss_window_create(delegate_c,
                          &box_c,
                          "C",
                          wuss_WINDOW_DEFAULT & ~wuss_WINDOW_BACK &
                          ~wuss_WINDOW_TOGGLE_SIZE & ~wuss_WINDOW_VSCROLL &
                          ~wuss_WINDOW_HSCROLL & ~wuss_WINDOW_RESIZE,
                          wuss_NO_BACKDROP,
                          box_size(&box_c),
                          SIZE2D(0, 0),
                          &win_c);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* C's titlebar, above its content, clear of the close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_c)
    goto Failure;

  wuss_window_close(win_c);

  rc = wuss_mouse_move(wuss, POINT(20, 20), &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: set_title truncates at WUSS_TITLE_MAX without splitting a "
         "multi-byte UTF-8 codepoint\n");

  {
    /* WUSS_TITLE_MAX - 2 ASCII bytes then a 3-byte UTF-8 codepoint (U+20AC,
     * EURO SIGN: 0xE2 0x82 0xAC) -- WUSS_TITLE_MAX lands on the euro sign's
     * middle continuation byte, so a raw byte truncation to
     * WUSS_TITLE_MAX chars (as strncpy alone would do) keeps 0xE2 0x82 and drops 0xAC,
     * leaving a malformed trailing sequence. The fix must back off over
     * both continuation bytes instead, dropping the whole codepoint. */
    static test_task_t tc_title;
    wuss_task_t       *delegate_title;
    wuss_window_t     *win_title;
    box_t              box_title;
    char               long_title[WUSS_TITLE_MAX + 8];
    const char        *got;
    size_t             len;

    tc_title.redraw_count = 0;
    tc_title.mouse_count  = 0;
    delegate_title = mk_task(wuss, test_handle, &tc_title);
    if (delegate_title == NULL) goto Failure;

    box_title.x0 = 10; box_title.y0 = 10;
    box_title.x1 = 50; box_title.y1 = 50;
    rc = wuss_window_create(delegate_title, &box_title, "T",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_title);
    if (rc != result_OK)
      goto Failure;

    memset(long_title, 'a', WUSS_TITLE_MAX - 2);
    long_title[WUSS_TITLE_MAX - 2] = (char) 0xE2;
    long_title[WUSS_TITLE_MAX - 1] = (char) 0x82;
    long_title[WUSS_TITLE_MAX]     = (char) 0xAC;
    long_title[WUSS_TITLE_MAX + 1] = '\0';

    wuss_window_set_title(win_title, long_title);
    got = wuss_window_get_title(win_title);
    len = strlen(got);

    if (len > WUSS_TITLE_MAX - 2)
      goto Failure; /* the euro sign's lead byte (0xE2) must have been
                      * dropped along with its continuations, not kept
                      * dangling on its own */
    if (len != WUSS_TITLE_MAX - 2)
      goto Failure; /* the leading ASCII bytes must all survive */
    if (((unsigned char) got[WUSS_TITLE_MAX - 3]) != 'a')
      goto Failure;

    wuss_window_close(win_title);
  }



  wuss_destroy(wuss);

  free(pixels);

  return result_TEST_PASSED;


Failure:

  printf("wuss_test: failed (rc=0x%X)\n", rc);

  return result_TEST_FAILED;
}

#else /* !(WUSS_FURNITURE && WUSS_ICONS) */

/* Compact core test for builds with furniture and/or icons compiled out.
 * Exercises the chromeless window path: create, move, z-order, invalidate,
 * redraw and programmatic scroll. No titlebar, no scrollbars, no icons. */
result_t wuss_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  test_task_t    tc_a, tc_b;
  wuss_task_t   *delegate_a, *delegate_b;
  box_t          box_a, box_b, content;
  wuss_window_t *win_a, *win_b, *hit;
  point_t        scroll;

  /* Force a chromeless window even in a build that still has furniture, so the
   * assertions below (content box == visible box) hold in every config. */
  const wuss_window_flags_t chromeless = wuss_WINDOW_NO_TITLEBAR |
                                         wuss_WINDOW_NO_OUTLINE;

  NOT_USED(resources);

  rowbytes = 200 * 4;
  pixels = malloc((size_t) rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes, NULL,
                   pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  printf("test: wuss_create (core)\n");

  rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 0;
  rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_a, "toosmall", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP,
                          box_size(&box_a),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  memset(&tc_a, 0, sizeof(tc_a));
  memset(&tc_b, 0, sizeof(tc_b));
  delegate_a = mk_task(wuss, test_handle, &tc_a);
  if (delegate_a == NULL) goto Failure;
  delegate_b = mk_task(wuss, test_handle, &tc_b);
  if (delegate_b == NULL) goto Failure;

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 100;
  rc = wuss_window_create(delegate_a, &box_a, "A", chromeless,
                          wuss_NO_BACKDROP,
                          SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_OK)
    goto Failure;

  box_b.x0 = 50; box_b.y0 = 50; box_b.x1 = 150; box_b.y1 = 150;
  rc = wuss_window_create(delegate_b, &box_b, "B", chromeless,
                          wuss_NO_BACKDROP,
                          SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_b);
  if (rc != result_OK)
    goto Failure;

  /* With furniture off the content box is the visible box verbatim. */
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x0 != 0 || content.y0 != 0 ||
      content.x1 != 100 || content.y1 != 100)
    goto Failure;

  printf("test: window_resize caps the size at the screen edge\n");

  /* screen is 200x200; win_a sits at (0,0), chromeless. asking for a
   * 500x500 content area is capped to the whole screen. */
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 200 || content.y1 - content.y0 != 200)
    goto Failure;

  /* a smaller request is likewise honoured verbatim */
  rc = wuss_window_resize(win_a, SIZE2D(120, 90));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 120 || content.y1 - content.y0 != 90)
    goto Failure;

  /* an offset top-left caps the size to the room left on screen */
  wuss_window_move(win_a, POINT(60, 40));
  rc = wuss_window_resize(win_a, SIZE2D(500, 500));
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x1 - content.x0 != 140 || content.y1 - content.y0 != 160)
    goto Failure;
  wuss_window_move(win_a, POINT(0, 0));
  rc = wuss_window_resize(win_a, SIZE2D(100, 100));
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create can never make a window bigger than the "
         "screen\n");

  {
    box_t          box_big;
    wuss_window_t *win_big;

    /* chromeless and the on-screen nudge drags the top-left back to (0,0),
     * so the clamp caps content at the full 200x200 screen */
    box_big.x0 = 10; box_big.y0 = 10; box_big.x1 = 400; box_big.y1 = 400;
    rc = wuss_window_create(mk_task(wuss, NULL, NULL), &box_big, "BIG", chromeless,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400),
                            SIZE2D(0, 0), &win_big);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_content_bounds(win_big, &content);
    if (content.x1 - content.x0 != 200 || content.y1 - content.y0 != 200)
      goto Failure;
    wuss_window_close(win_big);
  }

  printf("test: redraw delivers REDRAW events\n");

  wuss_redraw(wuss);
  if (tc_a.redraw_count == 0 || tc_b.redraw_count == 0)
    goto Failure;

  printf("test: z-order - click routes to topmost window\n");

  /* B was created last so it is on top over the overlap at (75,75). */
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;

  /* a press is captured until released; release so the next press re-routes */
  wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP,
                   NULL);

  wuss_window_restack(win_a, wuss_ZORDER_FRONT);
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1)
    goto Failure;
  wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT, wuss_MOUSE_UP,
                   NULL);

  printf("test: mouse point is in document coordinates\n");

  if (tc_a.last_x != 75 || tc_a.last_y != 75)
    goto Failure;

  printf("test: window_move\n");

  wuss_window_move(win_a, POINT(20, 20));
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x0 != 20 || content.y0 != 20)
    goto Failure;

  printf("test: programmatic scroll offsets document coordinates\n");

  wuss_window_set_scroll(win_a, POINT(10, 5));
  wuss_window_get_scroll(win_a, &scroll);
  if (scroll.x != 10 || scroll.y != 5)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(20, 20), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_a)
    goto Failure;
  if (tc_a.last_x != 10 || tc_a.last_y != 5)
    goto Failure;
  wuss_mouse_click(wuss, POINT(20, 20), wuss_BUTTON_SELECT, wuss_MOUSE_UP,
                   NULL);

  printf("test: wheel scroll routes to the window but a NO_VSCROLL window "
         "does not move\n");

  /* chromeless carries NO_VSCROLL, so wuss__scroll_step suppresses the wheel
   * step: the window is still the hit target but its offset is unchanged. */
  wuss_scroll(wuss, POINT(20, 20), 8, &hit);
  if (hit != win_a)
    goto Failure;
  wuss_window_get_scroll(win_a, &scroll);
  if (scroll.y != 5)
    goto Failure;

  printf("test: invalidate marks dirty region\n");

  wuss_window_invalidate(win_a, NULL);
  if (wuss_get_dirty_count(wuss) == 0)
    goto Failure;
  wuss_redraw(wuss);
  if (wuss_get_dirty_count(wuss) != 0)
    goto Failure;

  printf("test: window_close delivers CLOSE and drops the window\n");

  wuss_window_close(win_b);
  rc = wuss_mouse_click(wuss, POINT(140, 140), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != NULL)
    goto Failure;

  wuss_destroy(wuss);
  free(pixels);

  return result_TEST_PASSED;

Failure:

  printf("wuss_test: failed\n");

  return result_TEST_FAILED;
}

#endif /* WUSS_FURNITURE && WUSS_ICONS */
