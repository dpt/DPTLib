/* wuss-test.c -- wuss - minimal window manager */

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "wuss/wuss.h"
#include "wuss/window.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

typedef struct test_client
{
  int                 redraw_count;
  int                 mouse_count;
  wuss_mouse_action_t last_action;
  int                 last_x, last_y;
  wuss_button_t       last_button;
}
test_client_t;

static result_t test_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  test_client_t *tc;

  NOT_USED(window);
  NOT_USED(scr);
  NOT_USED(content);

  tc = client_data;
  tc->redraw_count++;

  return result_OK;
}

static result_t test_mouse(wuss_window_t *window, wuss_mouse_action_t action, int x, int y, wuss_button_t button, void *client_data)
{
  test_client_t *tc;

  NOT_USED(window);

  tc = client_data;
  tc->mouse_count++;
  tc->last_action = action;
  tc->last_x      = x;
  tc->last_y      = y;
  tc->last_button = button;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

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
  test_client_t  tc_a, tc_b, tc_c;
  wuss_client_t  client_a, client_b, client_c;
  box_t          box_a, box_b, box_c;
  wuss_window_t *win_a, *win_b, *win_c;
  wuss_window_t *hit;
  box_t          visible;
  int            before_a, before_b;
  int            width, height;
  const colour_t custom_palette[2] = { 0, 0 };

  NOT_USED(resources);

  rowbytes = 200 * 4;
  pixels = malloc(rowbytes * 200);
  if (pixels == NULL)
    goto Failure;

  rc = bitmap_init(&bm, 200, 200, pixelfmt_bgrx8888, rowbytes, NULL, pixels);
  if (rc != result_OK)
    goto Failure;

  screen_for_bitmap(&scr, &bm);

  printf("test: wuss_create with bad titlebar colour index\n");

  bad_config.titlebar_height = 0;
  bad_config.titlebar_bg     = 999;
  bad_config.titlebar_fg     = 0;
  rc = wuss_create(&scr, NULL, NULL, 0, &bad_config, &bad_wuss);
  if (rc != result_WUSS_BAD_COLOUR)
    goto Failure;

  printf("test: wuss_create with custom palette, no config\n");

  {
    wuss_t *custom_wuss;

    rc = wuss_create(&scr, NULL, custom_palette, 2, NULL, &custom_wuss);
    if (rc != result_OK)
      goto Failure;
    wuss_destroy(custom_wuss);
  }

  printf("test: wuss_create with default palette\n");

  rc = wuss_create(&scr, NULL, NULL, 0, NULL, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: window_create too small\n");

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 10; /* not taller than titlebar_height (20) */
  rc = wuss_window_create(wuss, &box_a, "toosmall", NULL, &win_a);
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  printf("test: create overlapping windows A and B\n");

  tc_a.redraw_count = 0;
  tc_a.mouse_count  = 0;
  client_a.redraw      = test_redraw;
  client_a.mouse       = test_mouse;
  client_a.client_data = &tc_a;

  box_a.x0 = 0;
  box_a.y0 = 0;
  box_a.x1 = 100;
  box_a.y1 = 100;
  rc = wuss_window_create(wuss, &box_a, "A", &client_a, &win_a);
  if (rc != result_OK)
    goto Failure;

  tc_b.redraw_count = 0;
  tc_b.mouse_count  = 0;
  client_b.redraw      = test_redraw;
  client_b.mouse       = test_mouse;
  client_b.client_data = &tc_b;

  box_b.x0 = 50;
  box_b.y0 = 50;
  box_b.x1 = 150;
  box_b.y1 = 150;
  rc = wuss_window_create(wuss, &box_b, "B", &client_b, &win_b);
  if (rc != result_OK)
    goto Failure;

  printf("test: redraw\n");

  rc = wuss_redraw(wuss);
  if (rc != result_OK)
    goto Failure;
  if (tc_a.redraw_count != 1 || tc_b.redraw_count != 1)
    goto Failure;

  printf("test: z-order hit test and local coordinate translation (B on top)\n");

  tc_b.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 75, 75, wuss_BUTTON_LEFT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_b)
    goto Failure;
  if (tc_b.mouse_count != 1 || tc_a.mouse_count != 0)
    goto Failure;
  if (tc_b.last_action != wuss_MOUSE_DOWN || tc_b.last_x != 25 || tc_b.last_y != 5)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_LEFT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: click-to-front changes subsequent overlap hits\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 25, 25, wuss_BUTTON_LEFT, &hit); /* only within A */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;

  rc = wuss_mouse_up(wuss, 25, 25, wuss_BUTTON_LEFT, &hit);
  if (rc != result_OK)
    goto Failure;

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 75, 75, wuss_BUTTON_LEFT, &hit); /* now A is topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 1 || tc_a.last_x != 75 || tc_a.last_y != 55)
    goto Failure;

  rc = wuss_mouse_up(wuss, 75, 75, wuss_BUTTON_LEFT, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: titlebar click starts a drag, not delivered as content\n");

  tc_a.mouse_count = 0;
  rc = wuss_mouse_down(wuss, 10, 10, wuss_BUTTON_LEFT, &hit); /* A's titlebar, A already topmost */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.mouse_count != 0)
    goto Failure;

  printf("test: drag-move updates visible bounds and triggers redraw\n");

  before_a = tc_a.redraw_count;
  before_b = tc_b.redraw_count;
  rc = wuss_mouse_move(wuss, 30, 15, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.redraw_count != before_a + 1 || tc_b.redraw_count != before_b + 1)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 20 || visible.y0 != 5)
    goto Failure;

  printf("test: mouse-up ends the drag\n");

  before_a = tc_a.redraw_count;
  rc = wuss_mouse_up(wuss, 30, 15, wuss_BUTTON_LEFT, &hit);
  if (rc != result_OK)
    goto Failure;
  if (hit != win_a)
    goto Failure;
  if (tc_a.redraw_count != before_a + 1)
    goto Failure;

  rc = wuss_mouse_move(wuss, 200, 200, &hit); /* off all windows, drag must have ended */
  if (rc != result_OK)
    goto Failure;
  if (hit != NULL)
    goto Failure;

  wuss_window_get_visible_bounds(win_a, &visible);
  if (visible.x0 != 20 || visible.y0 != 5)
    goto Failure;

  printf("test: window_resize valid and too-small cases\n");

  rc = wuss_window_resize(win_a, 50, 10); /* not taller than titlebar_height */
  if (rc != result_WUSS_TOO_SMALL)
    goto Failure;

  rc = wuss_window_resize(win_a, 50, 50);
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_visible_bounds(win_a, &visible);
  width  = visible.x1 - visible.x0;
  height = visible.y1 - visible.y0;
  if (width != 50 || height != 50)
    goto Failure;

  printf("test: destroy mid-drag then move doesn't crash\n");

  tc_c.redraw_count = 0;
  tc_c.mouse_count  = 0;
  client_c.redraw      = test_redraw;
  client_c.mouse       = test_mouse;
  client_c.client_data = &tc_c;

  box_c.x0 = 0;
  box_c.y0 = 0;
  box_c.x1 = 40;
  box_c.y1 = 40;
  rc = wuss_window_create(wuss, &box_c, "C", &client_c, &win_c);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_down(wuss, 5, 5, wuss_BUTTON_LEFT, &hit); /* C's titlebar */
  if (rc != result_OK)
    goto Failure;
  if (hit != win_c)
    goto Failure;

  wuss_window_destroy(win_c);

  rc = wuss_mouse_move(wuss, 20, 20, &hit);
  if (rc != result_OK)
    goto Failure;

  printf("test: destroy\n");

  wuss_window_destroy(win_a);
  wuss_window_destroy(win_b);
  wuss_destroy(wuss);

  free(pixels);

  return result_TEST_PASSED;


Failure:

  printf("failed\n");

  return result_TEST_FAILED;
}
