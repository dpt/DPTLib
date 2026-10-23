/* icon/test/hover-test.c -- wuss icon hover tracking tests */

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

/* The menu tests cover a menu entry's hover highlight; these check the
 * hover bookkeeping that every icon type gets as the pointer moves, and
 * what clears it. */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

#define NBUTTONS 50

typedef struct hover_log
{
  int          count; /* wuss_EVENT_ICON events seen */
  wuss_icon_t *icon;   /* ...and the last one's icon */
}
hover_log_t;

static result_t hover_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data)
{
  hover_log_t *log = task_data;

  NOT_USED(window);

  if (event->kind == wuss_EVENT_ICON)
  {
    log->count++;
    log->icon = event->data.icon.icon;
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

static void move(wuss_t *wuss, point_t p)
{
  wuss_mouse_move(wuss, p, NULL);
}

/* Is "icon" the one hovered icon, as both the wuss and the icon see it? */
static int hovering(wuss_t *wuss, wuss_icon_t *icon)
{
  return wuss->hover.icon == icon && wuss__icon_hovered(icon);
}

/* ----------------------------------------------------------------------- */

result_t wuss_hover_test(const char *resources)
{
  result_t            rc;
  wuss_fixture_t      fixture;
  wuss_t             *wuss;
  static hover_log_t  log;
  wuss_task_t        *task;
  box_t               winbox;
  wuss_window_t      *window;
  wuss_icon_spec_t    spec;
  static wuss_icon_t *b[NBUTTONS];
  int                 i;
  wuss_task_t        *mute;
  wuss_window_t      *quiet;
  point_t             p;
  point_t             before, after;
  wuss_icon_t        *under;

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 400, 240);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  task = mk_task(wuss, hover_handle, &log);
  if (task == NULL)
    goto Failure;

  /* a column of buttons, taller than the window so it can scroll, with
   * empty content to their right */
  winbox.x0 = 10; winbox.y0 = 30; winbox.x1 = 230; winbox.y1 = 200;
  rc = wuss_window_create(task, &winbox, "H", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, SIZE2D(220, NBUTTONS * 20),
                          SIZE2D(0, 0), &window);
  if (rc != result_OK)
    goto Failure;

  for (i = 0; i < NBUTTONS; i++)
  {
    wuss_icon_spec_action(&spec, (box_t) { 0, i * 20, 100, i * 20 + 20 },
                          "B", 0);
    rc = wuss_icon_create(window, &spec, &b[i]);
    if (rc != result_OK)
      goto Failure;
  }

  /* a second window whose task has no handler */
  mute = mk_task(wuss, NULL, NULL);
  if (mute == NULL)
    goto Failure;

  winbox.x0 = 260; winbox.y0 = 30; winbox.x1 = 390; winbox.y1 = 200;
  rc = wuss_window_create(mute, &winbox, "Q", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, SIZE2D(130, 170), SIZE2D(0, 0),
                          &quiet);
  if (rc != result_OK)
    goto Failure;

  printf("test: the pointer hovers one icon at a time\n");

  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;

  log.count = 0;
  move(wuss, centre_of(window, b[0]));
  if (!hovering(wuss, b[0]) || wuss->hover.window != window)
    FAIL("hover b0");
  if (log.count != 1 || log.icon != b[0])
    FAIL("hover b0 sent no MOVE");

  move(wuss, centre_of(window, b[1]));
  if (!hovering(wuss, b[1]) || wuss__icon_hovered(b[0]))
    FAIL("hover b1");

  /* only menu entries look different when hovered, so no repaint */
  if (wuss_get_dirty_count(wuss) != 0)
    FAIL("button hover dirtied");

  /* staying on the same icon keeps it */
  p = centre_of(window, b[1]);
  move(wuss, POINT(p.x + 1, p.y));
  if (!hovering(wuss, b[1]))
    FAIL("hover b1 again");

  printf("test: leaving the icons, the content or the window clears it\n");

  log.count = 0;
  move(wuss, POINT(p.x + 100, p.y));
  if (wuss->hover.icon != NULL || wuss->hover.window != NULL ||
      wuss__icon_hovered(b[1]))
    FAIL("empty content");
  if (log.count != 0)
    FAIL("empty content sent an icon event");

  move(wuss, centre_of(window, b[1]));
  move(wuss, POINT(120, 20)); /* titlebar */
  if (wuss->hover.icon != NULL || wuss__icon_hovered(b[1]))
    FAIL("titlebar");

  move(wuss, centre_of(window, b[1]));
  move(wuss, POINT(0, 239)); /* desktop */
  if (wuss->hover.icon != NULL || wuss__icon_hovered(b[1]))
    FAIL("desktop");

  move(wuss, centre_of(window, b[1]));
  move(wuss, POINT(320, 100)); /* a window with no handler */
  if (wuss->hover.icon != NULL || wuss__icon_hovered(b[1]))
    FAIL("handlerless window");

  printf("test: deleting the hovered or pressed icon forgets it\n");

  move(wuss, centre_of(window, b[2]));
  wuss_icon_delete(window, b[2]);
  b[2] = NULL;
  if (wuss->hover.icon != NULL || wuss->hover.window != NULL)
    FAIL("deleted hovered icon");

  p = centre_of(window, b[3]);
  wuss_mouse_click(wuss, p, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
  if (wuss->pressed.icon != b[3] || !wuss__icon_pressed(b[3]))
    FAIL("press b3");
  wuss_icon_delete(window, b[3]);
  b[3] = NULL;
  if (wuss->pressed.icon != NULL || wuss->pressed.window != NULL)
    FAIL("deleted pressed icon");
  wuss_mouse_click(wuss, p, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);

  printf("test: moving off a pressed button releases it\n");

  wuss_mouse_click(wuss, centre_of(window, b[4]), wuss_BUTTON_SELECT,
                   wuss_MOUSE_DOWN, NULL);
  if (!wuss__icon_pressed(b[4]))
    FAIL("press b4");
  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;

  move(wuss, centre_of(window, b[5]));
  if (wuss__icon_pressed(b[4]) || wuss->pressed.icon != NULL ||
      wuss->pressed.window != NULL)
    FAIL("left b4 still pressed");
  if (wuss_get_dirty_count(wuss) == 0)
    FAIL("released b4 not repainted");

  /* moving back does not press it again */
  move(wuss, centre_of(window, b[4]));
  if (wuss__icon_pressed(b[4]) || !hovering(wuss, b[4]))
    FAIL("back over b4");
  wuss_mouse_click(wuss, centre_of(window, b[4]), wuss_BUTTON_SELECT,
                   wuss_MOUSE_UP, NULL);

  printf("test: scrolling re-hovers whatever is now under the pointer\n");

  p = centre_of(window, b[0]);
  move(wuss, p);
  if (!hovering(wuss, b[0]))
    FAIL("hover b0 before scroll");

  wuss_window_get_scroll(window, &before);
  rc = wuss_scroll(wuss, p, 200, NULL); /* past b2 and b3, now deleted */
  if (rc != result_OK)
    goto Failure;
  wuss_window_get_scroll(window, &after);
  if (after.y == before.y)
    FAIL("window did not scroll");

  under = wuss__icon_hit_test(window, wuss__screen_to_doc(window, p));
  if (under == NULL || under == b[0])
    FAIL("scroll left b0 under the pointer");
  if (!hovering(wuss, under) || wuss__icon_hovered(b[0]))
    FAIL("scroll did not re-hover");

  wuss_window_close(quiet);
  wuss_window_close(window);
  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_hover_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_hover_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
