/* icon/test/slider-test.c -- wuss slider icon tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "framebuf/screen.h"
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

/* icon-test.c covers the pixel/value mapping and the wheel; these drive a
 * slider with the mouse, as a user would, and check the value API. */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

typedef struct slider_log
{
  int                 count;  /* wuss_EVENT_ICON events seen */
  wuss_icon_t        *icon;   /* ...and the last one's details */
  wuss_mouse_action_t action;
  int                 value;
}
slider_log_t;

static result_t slider_handle(wuss_window_t      *window,
                              const wuss_event_t *event,
                              void               *task_data)
{
  slider_log_t *log = task_data;

  NOT_USED(window);

  if (event->kind == wuss_EVENT_ICON)
  {
    log->count++;
    log->icon   = event->data.icon.icon;
    log->action = event->data.icon.action;
    log->value  = event->data.icon.value;
  }

  return result_OK;
}

/* The on-screen groove of "icon". */
static void groove_of(wuss_window_t *window,
                      wuss_icon_t   *icon,
                      box_t         *groove)
{
  box_t   content, screen_box;
  point_t scroll;

  wuss_window_get_content_bounds(window, &content);
  wuss_window_get_scroll(window, &scroll);
  wuss__icon_box_to_screen(&content, scroll, &icon->spec.bbox, &screen_box);
  wuss__slider_groove_box(&screen_box, groove);
}

static void press(wuss_t *wuss, point_t p)
{
  wuss_mouse_click(wuss, p, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
}

static void release(wuss_t *wuss, point_t p)
{
  wuss_mouse_click(wuss, p, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
}

/* ----------------------------------------------------------------------- */

result_t wuss_slider_test(const char *resources)
{
  result_t            rc;
  wuss_fixture_t      fixture;
  wuss_t             *wuss;
  static slider_log_t log;
  wuss_task_t        *task;
  box_t               winbox;
  wuss_window_t      *window;
  wuss_icon_spec_t    spec;
  wuss_icon_t        *across, *up, *label;
  box_t               g;
  int                 mid;

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 240, 240);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  task = mk_task(wuss, slider_handle, &log);
  if (task == NULL)
    goto Failure;

  winbox.x0 = 10; winbox.y0 = 30; winbox.x1 = 230; winbox.y1 = 230;
  rc = wuss_window_create(task, &winbox, "S", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                          &window);
  if (rc != result_OK)
    goto Failure;

  /* grooves 100 pixels long over 0..100, so a pixel is a unit */
  wuss_icon_spec_slider(&spec, (box_t) { 0, 0, 108, 20 },
                        wuss_SLIDER_HORIZONTAL, 0, 100, 50);
  rc = wuss_icon_create(window, &spec, &across);
  if (rc != result_OK) goto Failure;
  wuss_icon_spec_slider(&spec, (box_t) { 120, 0, 140, 108 },
                        wuss_SLIDER_VERTICAL, 0, 100, 50);
  rc = wuss_icon_create(window, &spec, &up);
  if (rc != result_OK) goto Failure;
  wuss_icon_spec_label(&spec, (box_t) { 0, 40, 60, 56 }, "L", 0);
  rc = wuss_icon_create(window, &spec, &label);
  if (rc != result_OK) goto Failure;

  printf("test: set and get value: clamping, no-op sets, non-sliders\n");

  if (wuss_icon_get_value(across) != 50)
    FAIL("initial value");

  wuss_icon_set_value(window, across, 150);
  if (wuss_icon_get_value(across) != 100) FAIL("clamp to max");
  wuss_icon_set_value(window, across, -5);
  if (wuss_icon_get_value(across) != 0) FAIL("clamp to min");

  /* setting the value it already has repaints nothing */
  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK) goto Failure;
  wuss_icon_set_value(window, across, 0);
  if (wuss_get_dirty_count(wuss) != 0) FAIL("no-op set dirtied");
  wuss_icon_set_value(window, across, 1);
  if (wuss_get_dirty_count(wuss) == 0) FAIL("set did not dirty");

  /* non-sliders have no value and ignore a set */
  wuss_icon_set_value(window, label, 7);
  if (wuss_icon_get_value(label) != 0 || label->value != 0)
    FAIL("label took a value");

  printf("test: a press jumps to the pointer, a drag tracks it, a release ends it\n");

  groove_of(window, across, &g);
  mid = (g.y0 + g.y1) / 2;

  log.count = 0;
  press(wuss, POINT(g.x0 + 75, mid));
  if (wuss_icon_get_value(across) != 75 || !wuss__icon_pressed(across))
    FAIL("press");

  log.count = 0;
  wuss_mouse_move(wuss, POINT(g.x0 + 25, mid), NULL);
  if (wuss_icon_get_value(across) != 25 || log.count != 1 ||
      log.icon != across || log.action != wuss_MOUSE_MOVE || log.value != 25)
    FAIL("drag");

  /* the drag keeps tracking outside the icon (though not outside the
   * window's content), clamped to the ends */
  wuss_mouse_move(wuss, POINT(g.x1 + 30, g.y1 + 40), NULL);
  if (wuss_icon_get_value(across) != 100 || log.value != 100)
    FAIL("drag past the right end");
  wuss_mouse_move(wuss, POINT(g.x0 - 2, g.y1 + 40), NULL);
  if (wuss_icon_get_value(across) != 0 || !wuss__icon_pressed(across))
    FAIL("drag past the left end");

  release(wuss, POINT(g.x0 + 10, mid));
  if (wuss__icon_pressed(across) || wuss->pressed.icon != NULL)
    FAIL("release");

  /* once released, hovering over the slider neither moves it nor tells the
   * task */
  log.count = 0;
  wuss_mouse_move(wuss, POINT(g.x0 + 90, mid), NULL);
  if (wuss_icon_get_value(across) != 0 || log.count != 0)
    FAIL("hover after release");

  printf("test: a vertical slider's maximum is at the top\n");

  groove_of(window, up, &g);
  mid = (g.x0 + g.x1) / 2;

  press(wuss, POINT(mid, g.y0));
  if (wuss_icon_get_value(up) != 100) FAIL("press at the top");
  wuss_mouse_move(wuss, POINT(mid, g.y1 - 30), NULL);
  if (wuss_icon_get_value(up) != 30 || log.icon != up) FAIL("drag down");
  wuss_mouse_move(wuss, POINT(mid, g.y1 + 50), NULL);
  if (wuss_icon_get_value(up) != 0) FAIL("drag past the bottom");
  release(wuss, POINT(mid, g.y1 + 50));

  printf("test: min > max runs the slider backwards\n");

  wuss_icon_delete(window, across);
  wuss_icon_spec_slider(&spec, (box_t) { 0, 0, 108, 20 },
                        wuss_SLIDER_HORIZONTAL, 100, 0, 0);
  rc = wuss_icon_create(window, &spec, &across);
  if (rc != result_OK) goto Failure;

  /* still clamped to the range, whichever end is which */
  wuss_icon_set_value(window, across, 200);
  if (wuss_icon_get_value(across) != 100) FAIL("reversed clamp");

  groove_of(window, across, &g);
  mid = (g.y0 + g.y1) / 2;
  press(wuss, POINT(g.x0 + 20, mid));
  if (wuss_icon_get_value(across) != 80) FAIL("reversed press");
  wuss_mouse_move(wuss, POINT(g.x1, mid), NULL);
  if (wuss_icon_get_value(across) != 0) FAIL("reversed drag to the right");
  release(wuss, POINT(g.x1, mid));

  wuss_window_close(window);
  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_slider_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_slider_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
