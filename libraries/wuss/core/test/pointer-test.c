/* wuss/core/test/pointer-test.c -- wuss pointer routing tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "wuss/task.h"
#include "wuss/window.h"

#include "test/all-tests.h"

#ifdef WUSS_FURNITURE

#include "../impl.h"
#include "../../test/harness.h"

#ifdef WUSS_ICONS
#include "wuss/icon-spec.h"
#include "../../icon.h"
#endif

/* ----------------------------------------------------------------------- */

result_t wuss_pointer_test(const char *resources)
{
  result_t       rc;
  int            rowbytes;
  void          *pixels;
  bitmap_t       bm;
  screen_t       scr;
  wuss_t        *wuss;
  test_task_t    tc;
  wuss_task_t   *task;
  wuss_window_t *win;
  box_t          box;
  box_t          content;
  box_t          well;
  box_t          sausage;
  point_t        at;
  point_t        scroll;
  int            page;

  NOT_USED(resources);

  rowbytes = 200 * 4;
  pixels   = malloc((size_t) rowbytes * 200);
  if (pixels == NULL)
    return result_TEST_FAILED;

  rc = bitmap_init(&bm, SIZE2D(200, 200), pixelfmt_bgrx8888, rowbytes, NULL,
                   pixels);
  if (rc != result_OK)
    goto Fail;
  screen_for_bitmap(&scr, &bm);

  rc = wuss_create(&scr, NULL, 0, NULL, 0, NULL, NULL, NULL, &wuss);
  if (rc != result_OK)
    goto Fail;

  forget_test_tasks();
  memset(&tc, 0, sizeof(tc));
  task = mk_task(wuss, test_handle, &tc);
  if (task == NULL)
    goto Fail;

  box.x0 = 5; box.y0 = 5; box.x1 = 125; box.y1 = 85;
  rc = wuss_window_create(task, &box, "PT", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, SIZE2D(400, 400), SIZE2D(0, 0),
                          &win);
  if (rc != result_OK)
    goto Fail;

  wuss_window_get_content_bounds(win, &content);

  printf("test: a click in the vertical well below the sausage pages down, "
         "Adjust pages back, and neither arms a sausage drag\n");
  {
    wuss__vscroll_well_box(win, &well);
    wuss__vscroll_sausage_box(win, &sausage);
    page = MAX((content.y1 - content.y0) - WUSS_SCROLL_STEP, 1);
    at   = POINT((well.x0 + well.x1) / 2, well.y1 - 1);

    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_window_get_scroll(win, &scroll);
    if (scroll.y != page || scroll.x != 0) goto Check;

    wuss_mouse_move(wuss, POINT(at.x, at.y - 10), NULL);
    wuss_window_get_scroll(win, &scroll);
    if (scroll.y != page) goto Check; /* paged, so no live drag */
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);

    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, NULL);
    wuss_window_get_scroll(win, &scroll);
    if (scroll.y != 0) goto Check;
    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, NULL);
  }

  printf("test: a click in the horizontal well right of the sausage pages "
         "right\n");
  {
    wuss__hscroll_well_box(win, &well);
    wuss__hscroll_sausage_box(win, &sausage);
    page = MAX((content.x1 - content.x0) - WUSS_SCROLL_STEP, 1);
    at   = POINT(well.x1 - 1, (well.y0 + well.y1) / 2);

    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    wuss_window_get_scroll(win, &scroll);
    if (scroll.x != page || scroll.y != 0) goto Check;
  }

#ifdef WUSS_ICONS
  wuss_window_set_scroll(win, POINT(0, 0));

  printf("test: option icon latches on a completed click; a lone radio "
         "toggles on Adjust only; a press released elsewhere is dropped "
         "without latching\n");
  {
    wuss_icon_spec_t spec;
    wuss_icon_t     *opt;
    wuss_icon_t     *radio;
    box_t            sbox;

    wuss_icon_spec_option(&spec, (box_t) BOX_POS_SIZE(0, 0, 40, 16), "o");
    rc = wuss_icon_create(win, &spec, &opt);
    if (rc != result_OK) goto Fail;

    wuss_icon_spec_radio(&spec, (box_t) BOX_POS_SIZE(0, 20, 40, 16), "r", 0);
    rc = wuss_icon_create(win, &spec, &radio);
    if (rc != result_OK) goto Fail;

    wuss_window_get_content_bounds(win, &content);
    wuss_window_get_scroll(win, &scroll);

    wuss_icon_get_bbox(opt, &box);
    wuss__icon_box_to_screen(&content, scroll, &box, &sbox);
    at = POINT((sbox.x0 + sbox.x1) / 2, (sbox.y0 + sbox.y1) / 2);

    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    if (!wuss__icon_pressed(opt) || wuss__icon_selected(opt)) goto Check;
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (wuss__icon_pressed(opt) || !wuss__icon_selected(opt)) goto Check;

    /* release off the window: pressed state drops, selection untouched */
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(190, 190), wuss_BUTTON_SELECT,
                     wuss_MOUSE_UP, NULL);
    if (wuss__icon_pressed(opt) || !wuss__icon_selected(opt)) goto Check;

    wuss_icon_get_bbox(radio, &box);
    wuss__icon_box_to_screen(&content, scroll, &box, &sbox);
    at = POINT((sbox.x0 + sbox.x1) / 2, (sbox.y0 + sbox.y1) / 2);

    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (!wuss__icon_selected(radio)) goto Check;
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (!wuss__icon_selected(radio)) goto Check; /* Select never clears */
    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at, wuss_BUTTON_ADJUST, wuss_MOUSE_UP, NULL);
    if (wuss__icon_selected(radio)) goto Check;
  }
#endif

  reap_test_tasks();
  wuss_destroy(wuss);
  free(pixels);
  return result_TEST_PASSED;

Check:
  printf("wuss_test: pointer check failed (scroll %d,%d)\n", scroll.x,
         scroll.y);
Fail:
  reap_test_tasks();
  if (wuss != NULL)
    wuss_destroy(wuss);
  free(pixels);
  return result_TEST_FAILED;
}

#else

result_t wuss_pointer_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
