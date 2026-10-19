/* icon/test/click-test.c -- wuss icon click tests */

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

/* Press and release over work-area icons: which events the task sees, and
 * how option, radio and grouped bitmap icons latch. */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

typedef struct click_log
{
  int                 icons;  /* wuss_EVENT_ICON events seen */
  int                 mice;   /* wuss_EVENT_MOUSE events seen */
  wuss_icon_t        *icon;   /* the last icon event's details */
  wuss_mouse_action_t action;
  wuss_button_t       button;
}
click_log_t;

static result_t click_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data)
{
  click_log_t *log = task_data;

  NOT_USED(window);

  if (event->kind == wuss_EVENT_ICON)
  {
    log->icons++;
    log->icon   = event->data.icon.icon;
    log->action = event->data.icon.action;
    log->button = event->data.icon.button;
  }
  else if (event->kind == wuss_EVENT_MOUSE)
  {
    log->mice++;
  }

  return result_OK;
}

/* The screen point at the centre of "icon". */
static point_t centre_of(wuss_window_t *window, wuss_icon_t *icon)
{
  box_t   content, screen_box;
  point_t scroll;

  wuss_window_get_content_bounds(window, &content);
  wuss_window_get_scroll(window, &scroll);
  wuss__icon_box_to_screen(&content, scroll, &icon->spec.bbox, &screen_box);

  return POINT((screen_box.x0 + screen_box.x1) / 2,
               (screen_box.y0 + screen_box.y1) / 2);
}

static void down(wuss_t *wuss, point_t p, wuss_button_t button)
{
  wuss_mouse_click(wuss, p, button, wuss_MOUSE_DOWN, NULL);
}

static void up(wuss_t *wuss, point_t p, wuss_button_t button)
{
  wuss_mouse_click(wuss, p, button, wuss_MOUSE_UP, NULL);
}

/* A full press and release of "button" over "icon". */
static void click(wuss_t        *wuss,
                  wuss_window_t *window,
                  wuss_icon_t   *icon,
                  wuss_button_t  button)
{
  point_t p;

  p = centre_of(window, icon);
  down(wuss, p, button);
  up(wuss, p, button);
}

static void spec_bitmap(wuss_icon_spec_t *spec, box_t bbox, int group)
{
  memset(spec, 0, sizeof(*spec));
  spec->type              = wuss_ICON_TYPE_BITMAP;
  spec->u.bitmap.name     = "nosuchicon";
  spec->u.bitmap.group    = group;
  spec->bbox              = bbox;
  spec->fg                = wuss_COLOUR_BLACK;
  spec->bg                = wuss_NO_BACKGROUND;
  spec->flags             = wuss_ICON_FLAGS_INTERACTIVE;
}

/* ----------------------------------------------------------------------- */

result_t wuss_click_test(const char *resources)
{
  result_t           rc;
  wuss_fixture_t     fixture;
  wuss_t            *wuss;
  static click_log_t log;
  wuss_task_t       *task;
  box_t              winbox;
  wuss_window_t     *window;
  wuss_icon_spec_t   spec;
  wuss_icon_t       *button, *option;
  wuss_icon_t       *lone, *r1, *r2;
  wuss_icon_t       *plain, *b1, *b2;
  wuss_icon_t       *field;
  point_t            p;

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 240, 240);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  task = mk_task(wuss, click_handle, &log);
  if (task == NULL)
    goto Failure;

  /* not focusable, so a writable here never takes the caret */
  winbox.x0 = 10; winbox.y0 = 30; winbox.x1 = 230; winbox.y1 = 230;
  rc = wuss_window_create(task, &winbox, "C", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                          &window);
  if (rc != result_OK)
    goto Failure;

  wuss_icon_spec_action(&spec, (box_t) { 0, 0, 60, 20 }, "B", 0);
  if ((rc = wuss_icon_create(window, &spec, &button)) != result_OK) goto Failure;
  wuss_icon_spec_option(&spec, (box_t) { 0, 30, 60, 50 }, "O");
  if ((rc = wuss_icon_create(window, &spec, &option)) != result_OK) goto Failure;
  wuss_icon_spec_radio(&spec, (box_t) { 0, 60, 60, 80 }, "L", 0);
  if ((rc = wuss_icon_create(window, &spec, &lone)) != result_OK) goto Failure;
  wuss_icon_spec_radio(&spec, (box_t) { 70, 60, 130, 80 }, "1", 1);
  if ((rc = wuss_icon_create(window, &spec, &r1)) != result_OK) goto Failure;
  wuss_icon_spec_radio(&spec, (box_t) { 140, 60, 200, 80 }, "2", 1);
  if ((rc = wuss_icon_create(window, &spec, &r2)) != result_OK) goto Failure;
  spec_bitmap(&spec, (box_t) { 0, 90, 24, 114 }, 0);
  if ((rc = wuss_icon_create(window, &spec, &plain)) != result_OK) goto Failure;
  spec_bitmap(&spec, (box_t) { 30, 90, 54, 114 }, 2);
  if ((rc = wuss_icon_create(window, &spec, &b1)) != result_OK) goto Failure;
  spec_bitmap(&spec, (box_t) { 60, 90, 84, 114 }, 2);
  if ((rc = wuss_icon_create(window, &spec, &b2)) != result_OK) goto Failure;
  wuss_icon_spec_writable(&spec, (box_t) { 0, 130, 120, 150 }, "", 16, 0);
  if ((rc = wuss_icon_create(window, &spec, &field)) != result_OK) goto Failure;

  printf("test: a click is a press then a release, each told to the task\n");

  p = centre_of(window, button);

  memset(&log, 0, sizeof(log));
  down(wuss, p, wuss_BUTTON_SELECT);
  if (log.icons != 1 || log.icon != button ||
      log.action != wuss_MOUSE_DOWN || log.button != wuss_BUTTON_SELECT)
    FAIL("press event");
  if (!wuss__icon_pressed(button) || wuss->pressed.icon != button)
    FAIL("press state");

  up(wuss, p, wuss_BUTTON_SELECT);
  if (log.icons != 2 || log.action != wuss_MOUSE_UP)
    FAIL("release event");
  if (wuss__icon_pressed(button) || wuss->pressed.icon != NULL)
    FAIL("release state");

  printf("test: Menu over an icon is a plain mouse event\n");

  memset(&log, 0, sizeof(log));
  down(wuss, p, wuss_BUTTON_MENU);
  if (log.icons != 0 || log.mice != 1 || wuss__icon_pressed(button))
    FAIL("menu press");
  up(wuss, p, wuss_BUTTON_MENU);

  printf("test: releasing away from the pressed icon cancels it\n");

  /* onto empty content: the press is dropped and no icon hears the
   * release */
  memset(&log, 0, sizeof(log));
  down(wuss, p, wuss_BUTTON_SELECT);
  up(wuss, POINT(p.x + 120, p.y), wuss_BUTTON_SELECT);
  if (wuss__icon_pressed(button) || wuss->pressed.icon != NULL)
    FAIL("release on empty content");
  if (log.icons != 1 || log.mice != 1)
    FAIL("release on empty content events");

  /* onto another icon: that icon hears the release but, never having been
   * pressed, does not latch */
  memset(&log, 0, sizeof(log));
  down(wuss, p, wuss_BUTTON_SELECT);
  up(wuss, centre_of(window, option), wuss_BUTTON_SELECT);
  if (wuss__icon_pressed(button) || wuss->pressed.icon != NULL)
    FAIL("release on option left button pressed");
  if (log.icons != 2 || log.icon != option || log.action != wuss_MOUSE_UP)
    FAIL("release on option event");
  if (wuss_icon_get_selected(option))
    FAIL("unpressed option latched");

  printf("test: options toggle on Select or Adjust\n");

  click(wuss, window, option, wuss_BUTTON_SELECT);
  if (!wuss_icon_get_selected(option)) FAIL("option on");
  click(wuss, window, option, wuss_BUTTON_ADJUST);
  if (wuss_icon_get_selected(option)) FAIL("option off");

  printf("test: radios: Select selects, Adjust toggles only a lone one\n");

  click(wuss, window, lone, wuss_BUTTON_SELECT);
  click(wuss, window, lone, wuss_BUTTON_SELECT);
  if (!wuss_icon_get_selected(lone)) FAIL("lone radio select");
  click(wuss, window, lone, wuss_BUTTON_ADJUST);
  if (wuss_icon_get_selected(lone)) FAIL("lone radio adjust");

  click(wuss, window, r1, wuss_BUTTON_SELECT);
  if (!wuss_icon_get_selected(r1)) FAIL("r1 select");
  click(wuss, window, r1, wuss_BUTTON_ADJUST);
  if (!wuss_icon_get_selected(r1)) FAIL("grouped radio adjust cleared");
  click(wuss, window, r2, wuss_BUTTON_SELECT);
  if (!wuss_icon_get_selected(r2) || wuss_icon_get_selected(r1))
    FAIL("r2 select");

  printf("test: grouped bitmaps latch like radios, plain ones do not\n");

  memset(&log, 0, sizeof(log));
  click(wuss, window, plain, wuss_BUTTON_SELECT);
  if (wuss_icon_get_selected(plain) || log.icons != 2)
    FAIL("plain bitmap");

  click(wuss, window, b1, wuss_BUTTON_SELECT);
  if (!wuss_icon_get_selected(b1)) FAIL("b1 select");
  click(wuss, window, b2, wuss_BUTTON_ADJUST);
  if (!wuss_icon_get_selected(b2) || wuss_icon_get_selected(b1))
    FAIL("b2 select");

  /* without wuss_ICON_FLAGS_INTERACTIVE a bitmap is inert */
  wuss__icon_set_flag(window, plain, wuss_ICON_FLAGS_INTERACTIVE, 0);
  memset(&log, 0, sizeof(log));
  click(wuss, window, plain, wuss_BUTTON_SELECT);
  if (log.icons != 0 || log.mice != 2)
    FAIL("inert bitmap");

  printf("test: a writable in an unfocusable window takes no caret\n");

  click(wuss, window, field, wuss_BUTTON_SELECT);
  if (wuss->caret.icon != NULL)
    FAIL("caret placed");

  wuss_window_close(window);
  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_click_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_click_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
