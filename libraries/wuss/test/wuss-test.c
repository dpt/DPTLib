/* wuss/test/wuss-test.c -- wuss - minimal window manager */

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
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/window.h"
#ifdef WUSS_MENUS
#include "wuss/menu.h"
#endif

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* The SDL interactive driver that used to live here (spawn_* callbacks, the
 * launcher table and the run_wuss loop) is now the standalone `wuss` app
 * in apps/wuss/main.c. This file is the Wuss unit test only. */

/* ----------------------------------------------------------------------- */

typedef struct test_task
{
  int                 redraw_count;
  int                 mouse_count;
  wuss_mouse_action_t last_action;
  int                 last_x, last_y;
  wuss_button_t       last_button;
  int                 last_scroll_x, last_scroll_y;
  int                 close_count;
  int                 stop_count;
  int                 open_count;
}
test_task_t;

static result_t test_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  test_task_t *tc;

  NOT_USED(window);

  tc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    tc->redraw_count++;
    break;

  case wuss_EVENT_MOUSE:
    tc->mouse_count++;
    tc->last_action = event->data.mouse.action;
    tc->last_x      = event->data.mouse.point.x;
    tc->last_y      = event->data.mouse.point.y;
    tc->last_button = event->data.mouse.button;
    break;

  case wuss_EVENT_SCROLL:
    tc->last_scroll_x = event->data.scroll.point.x;
    tc->last_scroll_y = event->data.scroll.point.y;
    break;

  case wuss_EVENT_CLOSE:
    tc->close_count++;
    break;

  case wuss_EVENT_QUIT:
    tc->stop_count++;
    break;

  case wuss_EVENT_OPEN:
    tc->open_count++;
    break;

  default:
    break;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* Total area covered by the dirty list, counting overlapped pixels once.
 * Summing each region's area instead would double-count wherever two
 * invalidations overlap, which they legitimately do. */
static int dirty_union_area(wuss_t *wuss, const box_t *bounds)
{
  static unsigned char covered[512 * 512];

  box_t region;
  int   w, h, i, x, y, area;

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

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

result_t wuss_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  wuss_config_t  bad_config;
  wuss_t        *bad_wuss;
  test_task_t    tc_a, tc_b, tc_c, tc_d;
  wuss_task_t    delegate_a, delegate_b, delegate_c, delegate_d;
  box_t          box_a, box_b, box_c, box_d;
  wuss_window_t *win_a, *win_b, *win_c, *win_d;
  wuss_window_t *hit;
  box_t          visible, content;
  int            before_a, before_b;
  int            width, height;
  const colour_t custom_palette[2] = { 0, 0 };

  NOT_USED(resources);

  rowbytes = 200 * 4;
  pixels = malloc(rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes, NULL, pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  printf("test: wuss_create with bad titlebar colour index\n");

  bad_config.titlebar_height         = 0;
  bad_config.palette.title.bg        = 999;
  bad_config.palette.title.fg        = 0;
  bad_config.palette.back            = 0;
  bad_config.palette.close           = 0;
  bad_config.palette.toggle          = 0;
  bad_config.palette.resize          = 0;
  bad_config.palette.scroll.arrows   = 0;
  bad_config.palette.scroll.wells    = 0;
  bad_config.palette.scroll.sausages = 0;
  rc = wuss_create(&scr, NULL, NULL, 0, &bad_config, NULL, &bad_wuss);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  printf("test: wuss_create with custom palette, no config\n");

  {
    wuss_t *custom_wuss;

    rc = wuss_create(&scr, NULL, custom_palette, 2, NULL, NULL, &custom_wuss);
    if (rc != result_OK)
      goto Failure;
    wuss_destroy(custom_wuss);
  }

  printf("test: wuss_create with default palette\n");

  rc = wuss_create(&scr, NULL, NULL, 0, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 0; /* zero-height content is invalid regardless of furniture */
  rc = wuss_window_create(wuss,
                          &box_a,
                          "toosmall",
                          wuss_WINDOW_NONE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                          NULL,
                          box_size(&box_a),
                          SIZE2D(0, 0),
                          &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  tc_a.redraw_count = 0;
  tc_a.mouse_count  = 0;
  tc_a.open_count   = 0;
  delegate_a.handle    = test_handle;
  delegate_a.task_data = &tc_a;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(wuss,
                          &box_a,
                          "A",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                          &delegate_a,
                          box_size(&box_a),
                          SIZE2D(0, 0),
                          &win_a);
  if (rc != result_OK)
    goto Failure;

  tc_b.redraw_count = 0;
  tc_b.mouse_count  = 0;
  delegate_b.handle    = test_handle;
  delegate_b.task_data = &tc_b;

  box_b.x0 = 50;
  box_b.y0 = 50;
  box_b.x1 = 150;
  box_b.y1 = 150;
  rc = wuss_window_create(wuss,
                          &box_b,
                          "B",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                          &delegate_b,
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

  printf("test: clicking a window's close icon sends wuss_EVENT_CLOSE, not a drag\n");

  tc_a.close_count = 0;
  rc = wuss_mouse_click(wuss, POINT(6, 11), wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit); /* A's close icon */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.close_count != 1)
    goto Failure;

  rc = wuss_mouse_click(wuss, POINT(31, 36), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit); /* if the close click had started a drag, this would move A */
  if (rc != result_OK)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 0 || visible.y0 != 0)
    goto Failure; /* unmoved: no drag was started by the close click */

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

  printf("test: title-less, no-outline window has no furniture, so visible == content\n");

  tc_d.redraw_count = 0;
  tc_d.mouse_count  = 0;
  delegate_d.handle      = test_handle;
  delegate_d.task_data = &tc_d;

  box_d.x0 = 0;  box_d.y0 = 160;
  box_d.x1 = 30; box_d.y1 = 175; /* shorter than the 20px titlebar_height, still valid: no titlebar to fit */
  rc = wuss_window_create(wuss,
                          &box_d,
                          "ignored",
                          wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                          &delegate_d,
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
    test_task_t    tc_e, tc_f;
    wuss_task_t    delegate_e, delegate_f;
    box_t          box_e, box_f;
    wuss_window_t *win_e, *win_f;

    tc_e.redraw_count = 0;
    tc_e.mouse_count  = 0;
    delegate_e.handle    = test_handle;
    delegate_e.task_data = &tc_e;

    box_e.x0 = 100; box_e.y0 = 0;
    box_e.x1 = 150; box_e.y1 = 50;
    rc = wuss_window_create(wuss,
                            &box_e,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_e,
                            box_size(&box_e),
                            SIZE2D(0, 0),
                            &win_e);
    if (rc != result_OK)
      goto Failure;

    tc_f.redraw_count = 0;
    tc_f.mouse_count  = 0;
    delegate_f.handle    = test_handle;
    delegate_f.task_data = &tc_f;

    box_f.x0 = 130; box_f.y0 = 20;
    box_f.x1 = 180; box_f.y1 = 70;
    rc = wuss_window_create(wuss,
                            &box_f,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_f,
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

  rc = wuss_window_set_background(win_d, wuss_BACKDROP_COLOUR(999));
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  rc = wuss_window_set_background(win_d, wuss_BACKDROP_COLOUR(1));
  if (rc != result_OK)
    goto Failure;

  wuss_window_close(win_d);

  printf("test: moving/resizing a window entirely behind an occluder has no visible effect\n");

  {
    test_task_t    tc_h, tc_g;
    wuss_task_t    delegate_h, delegate_g;
    box_t          box_h, box_g;
    wuss_window_t *win_h, *win_g;
    int            before_h, before_g;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h.handle    = test_handle;
    delegate_h.task_data = &tc_h;

    box_h.x0 = 10; box_h.y0 = 10;
    box_h.x1 = 30; box_h.y1 = 30;
    rc = wuss_window_create(wuss,
                            &box_h,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_h,
                            box_size(&box_h),
                            SIZE2D(0, 0),
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g.handle    = test_handle;
    delegate_g.task_data = &tc_g;

    box_g.x0 = 0;   box_g.y0 = 0;
    box_g.x1 = 150; box_g.y1 = 150; /* G is created after H, so G is topmost and fully covers H */
    rc = wuss_window_create(wuss,
                            &box_g,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_g,
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
    test_task_t    tc_i, tc_j;
    wuss_task_t    delegate_i, delegate_j;
    box_t          box_i, box_j, dirty;
    wuss_window_t *win_i, *win_j;

    tc_i.redraw_count = 0;
    tc_i.mouse_count  = 0;
    delegate_i.handle    = test_handle;
    delegate_i.task_data = &tc_i;

    box_i.x0 = 0; box_i.y0 = 0;
    box_i.x1 = 100; box_i.y1 = 100;
    rc = wuss_window_create(wuss,
                            &box_i,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_i,
                            box_size(&box_i),
                            SIZE2D(0, 0),
                            &win_i);
    if (rc != result_OK)
      goto Failure;

    tc_j.redraw_count = 0;
    tc_j.mouse_count  = 0;
    delegate_j.handle    = test_handle;
    delegate_j.task_data = &tc_j;

    box_j.x0 = 50; box_j.y0 = 0;
    box_j.x1 = 150; box_j.y1 = 100; /* J created after I, so J is topmost, covering I's right half */
    rc = wuss_window_create(wuss,
                            &box_j,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_j,
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
    test_task_t    tc_m;
    wuss_task_t    delegate_m;
    box_t          box_m;
    wuss_window_t *win_m;
    int            before_m;

    tc_m.redraw_count = 0;
    tc_m.mouse_count  = 0;
    delegate_m.handle    = test_handle;
    delegate_m.task_data = &tc_m;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 60; box_m.y1 = 60; /* 50x50, fully on-screen, topmost (created last) */
    rc = wuss_window_create(wuss,
                            &box_m,
                            NULL,
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_m,
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

  printf("test: Adjust-click on a window's back icon brings it to front\n");

  {
    test_task_t    tc_g, tc_h;
    wuss_task_t    delegate_g, delegate_h;
    box_t          box_g, box_h;
    wuss_window_t *win_g, *win_h;

    tc_h.redraw_count = 0;
    tc_h.mouse_count  = 0;
    delegate_h.handle    = test_handle;
    delegate_h.task_data = &tc_h;

    box_h.x0 = 130; box_h.y0 = 50;
    box_h.x1 = 190; box_h.y1 = 100;
    rc = wuss_window_create(wuss,
                            &box_h,
                            "H",
                            wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_h,
                            box_size(&box_h),
                            SIZE2D(0, 0),
                            &win_h);
    if (rc != result_OK)
      goto Failure;

    tc_g.redraw_count = 0;
    tc_g.mouse_count  = 0;
    delegate_g.handle    = test_handle;
    delegate_g.task_data = &tc_g;

    box_g.x0 = 110; box_g.y0 = 30;
    box_g.x1 = 160; box_g.y1 = 80;
    rc = wuss_window_create(wuss,
                            &box_g,
                            "G",
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_g,
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
    test_task_t    tc_m;
    wuss_task_t    delegate_m;
    box_t          box_m, content, visible;
    wuss_window_t *win_m;

    tc_m.redraw_count    = 0;
    tc_m.mouse_count     = 0;
    delegate_m.handle    = test_handle;
    delegate_m.task_data = &tc_m;

    box_m.x0 = 10; box_m.y0 = 10;
    box_m.x1 = 210; box_m.y1 = 210; /* 200x200 content, floored at 80x60 */
    rc = wuss_window_create(wuss, &box_m, "M", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_m,
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

  printf("test: toggle-size blits rather than redrawing the whole window\n");

  {
    test_task_t    tc_t;
    wuss_task_t    delegate_t;
    box_t          box_t_win, before, after, titlebar, toggle;
    wuss_window_t *win_t;
    int            outline_px, titlebar_height, inset, icon;
    int            i, dirty_area, full_area, cx, cy, old_icon_x, old_icon_y, found;
    int            interior_x, interior_y, interior_dirty;
    int            old_vscroll_x, old_vscroll_y, old_vscroll_found;

    tc_t.redraw_count = 0;
    tc_t.mouse_count  = 0;
    delegate_t.handle    = test_handle;
    delegate_t.task_data = &tc_t;

    box_t_win.x0 = 10; box_t_win.y0 = 10;
    box_t_win.x1 = 50; box_t_win.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(wuss, &box_t_win, "T", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_t,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_t);
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
     * exact-edge-aligned boxes merge; see box_merge in invalidate.c) */
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
      goto Failure; /* maximizing must stay on-screen: bounded by what's left
                      * of the screen from T's own x0/y0 (10,10), not by the
                      * screen's full width/height as if T were at the origin */

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

  printf("test: toggle-size that forces a scroll re-clamp invalidates the content it's about to redraw at the new offset, not just the blit's edge sliver\n");

  {
    test_task_t    tc_r;
    wuss_task_t    delegate_r;
    box_t          box_r, before, titlebar, toggle;
    wuss_window_t *win_r;
    int            outline_px, titlebar_height, inset, icon;
    int            i, interior_x, interior_y, interior_dirty, cx, cy;

    tc_r.redraw_count = 0;
    tc_r.mouse_count  = 0;
    delegate_r.handle    = test_handle;
    delegate_r.task_data = &tc_r;

    box_r.x0 = 10; box_r.y0 = 10;
    box_r.x1 = 50; box_r.y1 = 50; /* 40x40 content; doc bigger than that, so it starts scrollable */
    rc = wuss_window_create(wuss, &box_r, "R", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_r,
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

  printf("test: wuss_WINDOW_NO_RESIZE_BLIT redraws the whole window instead of blitting\n");

  {
    test_task_t    tc_nb;
    wuss_task_t    delegate_nb;
    box_t          box_nb, before, titlebar, toggle;
    wuss_window_t *win_nb;
    int            outline_px, titlebar_height, inset, icon;
    int            i, cx, cy, interior_x, interior_y, interior_dirty;

    tc_nb.redraw_count = 0;
    tc_nb.mouse_count  = 0;
    delegate_nb.handle    = test_handle;
    delegate_nb.task_data = &tc_nb;

    box_nb.x0 = 10; box_nb.y0 = 10;
    box_nb.x1 = 50; box_nb.y1 = 50; /* 40x40 content, room to grow to a 200x200 doc */
    rc = wuss_window_create(wuss, &box_nb, "NB", wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_nb,
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

  printf("test: toggle-size maximize accounts for scrollbar furniture, not just outline/titlebar\n");

  {
    test_task_t    tc_u;
    wuss_task_t    delegate_u;
    box_t          box_u, ub, titlebar, toggle;
    wuss_window_t *win_u;
    int            outline_px, titlebar_height, inset, icon;
    int            cx, cy;

    tc_u.redraw_count = 0;
    tc_u.mouse_count  = 0;
    delegate_u.handle    = test_handle;
    delegate_u.task_data = &tc_u;

    box_u.x0 = 80; box_u.y0 = 80;
    box_u.x1 = 120; box_u.y1 = 120; /* 40x40 content */
    rc = wuss_window_create(wuss, &box_u, "U", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND), /* scrollbars on: carve.x/y = icon size */
                            &delegate_u,
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
    test_task_t    tc_v;
    wuss_task_t    delegate_v;
    box_t          box_v, vb, titlebar, toggle;
    wuss_window_t *win_v;
    int            outline_px, titlebar_height, inset, icon;
    int            cx, cy;

    tc_v.redraw_count = 0;
    tc_v.mouse_count  = 0;
    delegate_v.handle    = test_handle;
    delegate_v.task_data = &tc_v;

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
    rc = wuss_window_create(wuss, &box_v, "V", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_v,
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
      goto Failure; /* screen-limited width/height went negative (x0 is
                      * further right than the screen edge plus furniture
                      * can make room for), producing an inverted box --
                      * un-hit-testable forever after, since box_contains_point
                      * can never match x0>x1: the window is stuck, unclickable,
                      * unclosable. Must floor at WUSS_MIN_CONTENT like
                      * drag-resize.c does, even if that leaves the maximized
                      * window hanging off the visible screen. */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* toggle back: shrink. Confirm the window is still reachable at all --
     * this alone would already fail (rc != result_OK from a wrong hit) if
     * the icon had become permanently unhittable above. */
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
    if (rc != result_OK)
      goto Failure;
    if (hit != win_v)
      goto Failure;
    rc = wuss_mouse_click(wuss, POINT(cx, cy), wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_visible_bounds(win_v, &vb);
    if (vb.x1 <= vb.x0 || vb.y1 <= vb.y0)
      goto Failure;

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_v);
  }

  printf("test: dragging a back-most window with nothing above it still blits\n");

  {
    test_task_t    tc_k, tc_l;
    wuss_task_t    delegate_k, delegate_l;
    box_t          box_k, box_l;
    wuss_window_t *win_k, *win_l;
    int            before_k, before_l;

    tc_k.redraw_count = 0;
    tc_k.mouse_count  = 0;
    delegate_k.handle    = test_handle;
    delegate_k.task_data = &tc_k;

    box_k.x0 = 0; box_k.y0 = 140; /* clear of the still-open A/B windows above */
    box_k.x1 = 50; box_k.y1 = 175;
    rc = wuss_window_create(wuss,
                            &box_k,
                            "K",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_k,
                            box_size(&box_k),
                            SIZE2D(0, 0),
                            &win_k);
    if (rc != result_OK)
      goto Failure;

    tc_l.redraw_count = 0;
    tc_l.mouse_count  = 0;
    delegate_l.handle    = test_handle;
    delegate_l.task_data = &tc_l;

    box_l.x0 = 120; box_l.y0 = 140; /* well clear of K, so never overlaps it */
    box_l.x1 = 170; box_l.y1 = 175;
    rc = wuss_window_create(wuss,
                            &box_l,
                            "L",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_l,
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

  printf("test: resizing a window only invalidates the grown/shrunk sliver\n");

  {
    test_task_t    tc_m2;
    wuss_task_t    delegate_m2;
    box_t          box_m2, before2, after2, region;
    wuss_window_t *win_m2;
    int            i, dirty_area, full_area, interior_x, interior_y, interior_dirty;

    tc_m2.redraw_count = 0;
    tc_m2.mouse_count  = 0;
    delegate_m2.handle    = test_handle;
    delegate_m2.task_data = &tc_m2;

    box_m2.x0 = 0; box_m2.y0 = 0;
    box_m2.x1 = 40; box_m2.y1 = 40;
    rc = wuss_window_create(wuss,
                            &box_m2,
                            "M2",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_m2,
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
    test_task_t    tc_nb2;
    wuss_task_t    delegate_nb2;
    box_t          box_nb2, before3, after3, region;
    wuss_window_t *win_nb2;
    int            i, dirty_area, full_area;

    tc_nb2.redraw_count = 0;
    tc_nb2.mouse_count  = 0;
    delegate_nb2.handle    = test_handle;
    delegate_nb2.task_data = &tc_nb2;

    box_nb2.x0 = 0; box_nb2.y0 = 0;
    box_nb2.x1 = 40; box_nb2.y1 = 40;
    rc = wuss_window_create(wuss, &box_nb2, "NB2", wuss_WINDOW_NO_RESIZE_BLIT,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_nb2,
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

  printf("test: dragging a clear window onto an occluder leaves the occluder untouched\n");

  {
    test_task_t    tc_n, tc_o;
    wuss_task_t    delegate_n, delegate_o;
    box_t          box_n, box_o, visible_o, exposed, occluded, region;
    wuss_window_t *win_n, *win_o;
    int            i, exposed_dirty, occluded_dirty;

    tc_n.redraw_count = 0;
    tc_n.mouse_count  = 0;
    delegate_n.handle    = test_handle;
    delegate_n.task_data = &tc_n;

    box_n.x0 = 0; box_n.y0 = 140; /* clear of any occluder to start */
    box_n.x1 = 60; box_n.y1 = 170;
    rc = wuss_window_create(wuss, &box_n, "N",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_n,
                            box_size(&box_n),
                            SIZE2D(0, 0),
                            &win_n);
    if (rc != result_OK)
      goto Failure;

    tc_o.redraw_count = 0;
    tc_o.mouse_count  = 0;
    delegate_o.handle    = test_handle;
    delegate_o.task_data = &tc_o;

    box_o.x0 = 90; box_o.y0 = 140; /* N will be dragged partly on top of O */
    box_o.x1 = 130; box_o.y1 = 180;
    rc = wuss_window_create(wuss, &box_o, "O",
                            wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_o,
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
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_a, visible_b_before;
    box_t          clean_new, hidden_new, region;
    wuss_window_t *win_a, *win_b;
    int            i, dx, clean_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;

    box_b.x0 = 20; box_b.y0 = 10; /* left half will sit under A */
    box_b.x1 = 80; box_b.y1 = 50;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_b,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 40; box_a.y1 = 100;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_a,
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
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_a, visible_b_before;
    box_t          region;
    wuss_window_t *win_a, *win_b;
    int            i, occluder_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;

    box_b.x0 = 0; box_b.y0 = 0; /* right part sits under A throughout */
    box_b.x1 = 60; box_b.y1 = 40;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_b,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;

    box_a.x0 = 40; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 40;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_a,
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
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_b_before;
    box_t          occluded_overlap, hidden_new, region;
    wuss_window_t *win_a, *win_b;
    int            i, occluded_dirty, hidden_dirty;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;

    box_b.x0 = 0; box_b.y0 = 0; /* middle band sits under A */
    box_b.x1 = 60; box_b.y1 = 60;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_b,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;

    box_a.x0 = 0; box_a.y0 = 20; /* created after B, so A is topmost */
    box_a.x1 = 60; box_a.y1 = 40;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_a,
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
    test_task_t    tc_a, tc_b;
    wuss_task_t    delegate_a, delegate_b;
    box_t          box_a, box_b, visible_b_before, visible_a, region;
    wuss_window_t *win_a, *win_b;
    int            i;

    tc_b.redraw_count = 0;
    tc_b.mouse_count  = 0;
    delegate_b.handle    = test_handle;
    delegate_b.task_data = &tc_b;

    box_b.x0 = 80; box_b.y0 = 80; /* corner already under A */
    box_b.x1 = 140; box_b.y1 = 140;
    rc = wuss_window_create(wuss, &box_b, "B",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_b,
                            box_size(&box_b),
                            SIZE2D(0, 0),
                            &win_b);
    if (rc != result_OK)
      goto Failure;

    tc_a.redraw_count = 0;
    tc_a.mouse_count  = 0;
    delegate_a.handle    = test_handle;
    delegate_a.task_data = &tc_a;

    box_a.x0 = 0; box_a.y0 = 0; /* created after B, so A is topmost */
    box_a.x1 = 100; box_a.y1 = 100;
    rc = wuss_window_create(wuss, &box_a, "A",
                            wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE |
                            wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                            wuss_WINDOW_NO_RESIZE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_a,
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
      box_t  visible_b_after, whole_footprint, dirty_area_box;
      int    dirty_area, footprint_area;

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

  printf("test: destroy mid-drag then move doesn't crash\n");

  tc_c.redraw_count = 0;
  tc_c.mouse_count  = 0;
  delegate_c.handle      = test_handle;
  delegate_c.task_data = &tc_c;

  box_c.x0 = 0;
  box_c.y0 = 0;
  box_c.x1 = 40;
  box_c.y1 = 40;
  rc = wuss_window_create(wuss,
                          &box_c,
                          "C",
                          wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE |
                          wuss_WINDOW_NO_VSCROLL | wuss_WINDOW_NO_HSCROLL |
                          wuss_WINDOW_NO_RESIZE,
                          wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                          &delegate_c,
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

  printf("test: mouse and scroll events arrive in virtual content space, with the scroll offset applied exactly once\n");

  {
    test_task_t    tc_s = { 0 };
    wuss_task_t    delegate_s;
    box_t          box_s, content_s;
    wuss_window_t *win_s;
    point_t        scroll;

    delegate_s.handle    = test_handle;
    delegate_s.task_data = &tc_s;

    box_s.x0 = 10; box_s.y0 = 10;
    box_s.x1 = 60; box_s.y1 = 60; /* 50x50 content onto a 200x200 doc: room to scroll */
    rc = wuss_window_create(wuss, &box_s, "S", wuss_WINDOW_NONE,
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_s,
                            SIZE2D(200, 200), SIZE2D(0, 0), &win_s);
    if (rc != result_OK)
      goto Failure;

    scroll.x = 30;
    scroll.y = 40;
    wuss_window_set_scroll(win_s, scroll);
    wuss_window_get_scroll(win_s, &scroll); /* read back in case it clamped */

    wuss_window_get_content_bounds(win_s, &content_s);

    rc = wuss_mouse_click(wuss,
                          POINT(content_s.x0 + 5, content_s.y0 + 7),
                          wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;
    if (tc_s.last_x != 5 + scroll.x || tc_s.last_y != 7 + scroll.y)
      goto Failure; /* a task adding the scroll offset itself would double-count it */

    rc = wuss_mouse_click(wuss,
                          POINT(content_s.x0 + 5, content_s.y0 + 7),
                          wuss_BUTTON_SELECT, wuss_MOUSE_UP, &hit);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_mouse_move(wuss,
                         POINT(content_s.x0 + 11, content_s.y0 + 13),
                         &hit);
    if (rc != result_OK)
      goto Failure;
    if (tc_s.last_x != 11 + scroll.x || tc_s.last_y != 13 + scroll.y)
      goto Failure;

    tc_s.last_scroll_x = -1;
    tc_s.last_scroll_y = -1;
    rc = wuss_scroll(wuss,
                     POINT(content_s.x0 + 3, content_s.y0 + 4),
                     1, &hit);
    if (rc != result_OK)
      goto Failure;
    if (hit != win_s)
      goto Failure;
    if (tc_s.last_scroll_x != 3 + scroll.x || tc_s.last_scroll_y != 4 + scroll.y)
      goto Failure;

    wuss_window_close(win_s);
  }

  printf("test: content bounds survive furniture, including the interior rules\n");

  {
    /* Furniture -- outline, titlebar, scrollbars and the rules dividing the
     * content from them -- is added outside the requested content box, never
     * carved out of it, so what the caller asks for is what it gets, both at
     * creation and after a resize. */
    test_task_t    tc_r;
    wuss_task_t    delegate_r;
    box_t          box_r, content_r;
    wuss_window_t *win_r;

    memset(&tc_r, 0, sizeof(tc_r));
    delegate_r.handle    = test_handle;
    delegate_r.task_data = &tc_r;

    box_r.x0 = 20;
    box_r.y0 = 30;
    box_r.x1 = 120;
    box_r.y1 = 110;
    rc = wuss_window_create(wuss,
                            &box_r,
                            "rules",
                            wuss_WINDOW_NONE, /* all furniture present */
                            wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                            &delegate_r,
                            SIZE2D(400, 400),
                            SIZE2D(0, 0),
                            &win_r);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_r, &content_r);
    if (content_r.x1 - content_r.x0 != box_r.x1 - box_r.x0 ||
        content_r.y1 - content_r.y0 != box_r.y1 - box_r.y0)
      goto Failure;

    rc = wuss_window_resize(win_r, SIZE2D(61, 47));
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_r, &content_r);
    if (content_r.x1 - content_r.x0 != 61 || content_r.y1 - content_r.y0 != 47)
      goto Failure;

    wuss_window_close(win_r);
  }

  printf("test: wuss_window_create_placed tiles windows and reclaims a closed slot\n");

  {
    /* Windows created without a position are packed towards the top-left and
     * must not overlap; closing one frees its slot for the next create. */
    test_task_t    tc_p[4];
    wuss_task_t    delegate_p;
    wuss_window_t *win_p[4];
    box_t          vis[4], probe;
    int            k, m;

    memset(tc_p, 0, sizeof(tc_p));
    delegate_p.handle    = test_handle;
    delegate_p.task_data = &tc_p[0];

    for (k = 0; k < 4; k++)
    {
      rc = wuss_window_create_placed(wuss,
                                     SIZE2D(40, 30),
                                     "P",
                                     wuss_WINDOW_NONE,
                                     wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                     &delegate_p,
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

    rc = wuss_window_create_placed(wuss,
                                   SIZE2D(40, 30),
                                   "P",
                                   wuss_WINDOW_NONE,
                                   wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND),
                                   &delegate_p,
                                   SIZE2D(40, 30),
                                   SIZE2D(0, 0),
                                   &win_p[1]);
    if (rc != result_OK)
      goto Failure;
    wuss_window_get_visible_bounds(win_p[1], &vis[1]);
    if (vis[1].x0 != probe.x0 || vis[1].y0 != probe.y0 ||
        vis[1].x1 != probe.x1 || vis[1].y1 != probe.y1)
      goto Failure; /* freed slot not reused */

    /* a manual move releases the slot: closing afterwards must not
     * double-release (would corrupt the packer's free list) */
    wuss_window_move(win_p[0], POINT(200, 200));

    for (k = 0; k < 4; k++)
      wuss_window_close(win_p[k]);
  }

  printf("test: wuss_task_stop sends wuss_EVENT_QUIT to each window's task\n");

  tc_a.stop_count = 0;
  tc_b.stop_count = 0;
  wuss_task_stop(win_a);
  wuss_task_stop(win_b);
  wuss_window_close(win_a);
  wuss_window_close(win_b);
  if (tc_a.stop_count != 1 || tc_b.stop_count != 1)
    goto Failure;

#ifdef WUSS_MENUS
  printf("test: wuss_menu_create_from_desc parses a descriptor tree\n");
  {
    static const wuss_menu_item_t borrowed_items[] =
    {
      { "Info",  wuss_MENU_ITEM_NONE, NULL },
      { "About", wuss_MENU_ITEM_NONE, NULL }
    };
    static const wuss_menu_t borrowed =
    {
      "Help", borrowed_items, NELEMS(borrowed_items)
    };
    wuss_menu_t *m;
    const wuss_menu_t *sub;

    /* "Root" is the root caption (first token), then items App,
     * File{ %s title discarded, Grid(!), Export(~), |Quit }, Help(> borrowed),
     * with "File" substituted for both %s. '|Quit' sets DASHED on Quit itself
     * (rule drawn above it, text kept), so the submenu holds 3 rows. */
    rc = wuss_menu_create_from_desc(&m,
           "Root, App, %s { %s, !Grid, ~Export, |Quit }, >Help",
           "File", "File", &borrowed);
    if (rc != result_OK)
      goto Failure;

    if (m->nitems != 3)                                   goto MenuFail;
    if (m->title == NULL || strcmp(m->title, "Root") != 0) goto MenuFail;
    if (strcmp(m->items[0].text, "App") != 0)             goto MenuFail;
    if (m->items[0].submenu != NULL)                      goto MenuFail;

    /* items[1] "File" carries the { } submenu; its first token was the title
     * and is not emitted; '|Quit' keeps Quit's text and flags it DASHED */
    if (strcmp(m->items[1].text, "File") != 0)            goto MenuFail;
    sub = m->items[1].submenu;
    if (sub == NULL || sub->nitems != 3)                  goto MenuFail;
    if (strcmp(sub->items[0].text, "Grid") != 0)          goto MenuFail;
    if (!(sub->items[0].flags & wuss_MENU_ITEM_TICKED))   goto MenuFail;
    if (!(sub->items[1].flags & wuss_MENU_ITEM_DISABLED)) goto MenuFail;
    if (strcmp(sub->items[2].text, "Quit") != 0)          goto MenuFail;
    if (!(sub->items[2].flags & wuss_MENU_ITEM_DASHED))   goto MenuFail;

    /* items[2] "Help" got a deep copy of the borrowed menu */
    if (strcmp(m->items[2].text, "Help") != 0)            goto MenuFail;
    sub = m->items[2].submenu;
    if (sub == NULL || sub == &borrowed)                  goto MenuFail;
    if (sub->nitems != 2)                                 goto MenuFail;
    if (strcmp(sub->items[1].text, "About") != 0)         goto MenuFail;

    wuss_menu_destroy(m);

    /* malformed: unbalanced brace */
    rc = wuss_menu_create_from_desc(&m, "T, A { B, C");
    if (rc != result_BAD_ARG)
      goto MenuFail;

    rc = result_OK;
    goto MenuOK;

MenuFail:
    printf("wuss_test: menu-desc check failed\n");
    return result_TEST_FAILED;
MenuOK: ;
  }
#endif

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
  wuss_task_t    delegate_a, delegate_b;
  box_t          box_a, box_b, content;
  wuss_window_t *win_a, *win_b, *hit;
  point_t        scroll;

  /* Force a chromeless window even in a build that still has furniture, so the
   * assertions below (content box == visible box) hold in every config. */
  const wuss_window_flags_t chromeless =
    wuss_WINDOW_NO_TITLEBAR | wuss_WINDOW_NO_OUTLINE | wuss_WINDOW_NO_CLOSE |
    wuss_WINDOW_NO_BACK | wuss_WINDOW_NO_TOGGLE_SIZE | wuss_WINDOW_NO_VSCROLL |
    wuss_WINDOW_NO_HSCROLL | wuss_WINDOW_NO_RESIZE;

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

  rc = wuss_create(&scr, NULL, NULL, 0, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 0;
  rc = wuss_window_create(wuss, &box_a, "toosmall", wuss_WINDOW_NONE,
                          wuss_NO_BACKGROUND, NULL, box_size(&box_a),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  memset(&tc_a, 0, sizeof(tc_a));
  memset(&tc_b, 0, sizeof(tc_b));
  delegate_a.handle = test_handle; delegate_a.task_data = &tc_a;
  delegate_b.handle = test_handle; delegate_b.task_data = &tc_b;

  box_a.x0 = 0; box_a.y0 = 0; box_a.x1 = 100; box_a.y1 = 100;
  rc = wuss_window_create(wuss, &box_a, "A", chromeless,
                          wuss_NO_BACKGROUND, &delegate_a, SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_a);
  if (rc != result_OK)
    goto Failure;

  box_b.x0 = 50; box_b.y0 = 50; box_b.x1 = 150; box_b.y1 = 150;
  rc = wuss_window_create(wuss, &box_b, "B", chromeless,
                          wuss_NO_BACKGROUND, &delegate_b, SIZE2D(400, 400),
                          SIZE2D(0, 0), &win_b);
  if (rc != result_OK)
    goto Failure;

  /* With furniture off the content box is the visible box verbatim. */
  wuss_window_get_content_bounds(win_a, &content);
  if (content.x0 != 0 || content.y0 != 0 ||
      content.x1 != 100 || content.y1 != 100)
    goto Failure;

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

  wuss_window_restack(win_a, wuss_ZORDER_FRONT);
  rc = wuss_mouse_click(wuss, POINT(75, 75), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, &hit);
  if (rc != result_OK || hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1)
    goto Failure;

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

  printf("test: wheel scroll delivers SCROLL event and moves offset\n");

  wuss_scroll(wuss, POINT(20, 20), 8, &hit);
  if (hit != win_a)
    goto Failure;
  wuss_window_get_scroll(win_a, &scroll);
  if (scroll.y != 13)
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
