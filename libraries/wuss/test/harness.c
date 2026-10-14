/* wuss/test/harness.c -- shared fixtures for the wuss tests */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/message.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#ifdef WUSS_MENUS
#include "wuss/menu.h"
#endif

#include "harness.h"

/* ----------------------------------------------------------------------- */

result_t test_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  test_task_t *tc;

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

  case wuss_EVENT_CLOSE_ADJUST:
    tc->close_adjust_count++;
    tc->close_adjust_mods = event->data.close_adjust.modifiers;
    break;

  case wuss_EVENT_PRE_CLOSE:
    tc->pre_close_count++;
    if (tc->veto_pre_close)
      return result_BAD_ARG; /* any non-OK return vetoes the close */
    break;

  case wuss_EVENT_SHOW:
    tc->show_count++;
    break;

  case wuss_EVENT_PRE_SHOW:
    tc->pre_show_count++;
    if (tc->veto_pre_show)
      break; /* not calling wuss_window_reveal_now still shows: default proceed */
    if (tc->open_window_via_handle)
    {
      /* the correct guard: a plain window's PRE_SHOW carries handle == NULL
       * (already proceeding by default), and calling
       * wuss_menu_open_window_now with a NULL/non-menu handle is a caller
       * error -- regression coverage for a real crash where saturn.c and
       * apps/wuss/tasks.c called it unconditionally */
      if (event->data.pre_show.handle == NULL)
        return result_OK;
      return wuss_menu_open_window_now(event->data.pre_show.handle,
                                       event->data.pre_show.index);
    }
    return wuss_window_reveal_now(window);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    tc->pre_submenu_open_count++;
    if (tc->submenu_to_open == NULL)
      break; /* this only fires for a flagged row; not calling
              * wuss_menu_open_submenu_now leaves it inert */
    return wuss_menu_open_submenu_now(event->data.pre_submenu_open.handle,
                                      event->data.pre_submenu_open.index,
                                      tc->submenu_to_open);

  case wuss_EVENT_MENU_SELECT:
    tc->menu_select_count++;
    tc->last_menu_index = event->data.menu_select.index;
    break;

  case wuss_EVENT_QUIT:
    tc->stop_count++;
    break;

  case wuss_EVENT_OPEN:
    tc->open_count++;
    break;

  case wuss_EVENT_PALETTE:
    tc->palette_count++;
    break;

  case wuss_EVENT_IDLE:
    tc->idle_count++;
    break;

  case wuss_EVENT_GAIN_FOCUS:
    tc->gain_focus_count++;
    break;

  case wuss_EVENT_LOSE_FOCUS:
    tc->lose_focus_count++;
    break;

  case wuss_EVENT_ICON:
    tc->icon_count++;
    break;

  case wuss_EVENT_KEY:
    tc->key_count++;
    tc->last_key      = event->data.key.code;
    tc->last_key_mods = event->data.key.modifiers;
    if (tc->unclaim_keys)
      return result_WUSS_KEY_UNCLAIMED;
    break;

  case wuss_EVENT_DRAG_END:
    tc->drag_end_count++;
    tc->last_drag_end_drop      = event->data.drag_end.drop;
    tc->last_drag_end_x         = event->data.drag_end.point.x;
    tc->last_drag_end_y         = event->data.drag_end.point.y;
    tc->last_drag_end_cancelled = event->data.drag_end.cancelled;
    break;

  default:
    break;
  }

  return result_OK;
}

/* Most tests declare their test_task_t on the block stack. A registered
 * task outlives that block (wuss only sweeps at wuss_destroy), so once the
 * block exits the task's task_data dangles. Broadcast events (PALETTE,
 * IDLE) walk every registered task and would read that freed stack.
 *
 * mk_task records every task it makes; reap_test_tasks destroys them all.
 * Call reap_test_tasks() before any broadcast test and before
 * wuss_destroy, so no stale stack is ever delivered to. */
wuss_task_t *mk_task_reg[MK_TASK_MAX];
int          mk_task_count;

/* Register a task in one call. Returns NULL on OOM; callers goto Failure. */
wuss_task_t *mk_task(wuss_t           *wuss,
                     wuss_window_fn_t *handle,
                     void             *task_data)
{
  wuss_task_desc_t desc;
  wuss_task_t     *task;

  desc.handle    = handle;
  desc.task_data = task_data;
  desc.name      = "wuss-test";

  if (wuss_task_create(wuss, &desc, &task) != result_OK)
    return NULL;

  if (mk_task_count < MK_TASK_MAX)
    mk_task_reg[mk_task_count++] = task;

  return task;
}

/* Destroy every task mk_task created and forget them. Each
 * wuss_task_destroy closes that task's windows and fires one QUIT. */
void reap_test_tasks(void)
{
  int i;

  for (i = 0; i < mk_task_count; i++)
    wuss_task_destroy(mk_task_reg[i]);
  mk_task_count = 0;
}

/* Drop the registry without destroying anything -- for a block that hands
 * its tasks straight to wuss_destroy and wants that teardown path exercised
 * rather than wuss_task_destroy's. */
void forget_test_tasks(void)
{
  mk_task_count = 0;
}

/* ----------------------------------------------------------------------- */

result_t wuss_fixture_open(wuss_fixture_t *fixture,
                           int             width,
                           int             height)
{
  result_t rc;
  int      rowbytes;

  fixture->wuss = NULL;

  forget_test_tasks();

  rowbytes        = width * 4;
  fixture->pixels = malloc((size_t) rowbytes * height);
  if (fixture->pixels == NULL)
    return result_OOM;

  rc = bitmap_init(&fixture->bm, SIZE2D(width, height), pixelfmt_bgrx8888,
                   rowbytes, NULL, fixture->pixels);
  if (rc != result_OK)
    return rc;

  screen_for_bitmap(&fixture->scr, &fixture->bm);

  return wuss_create(&fixture->scr, NULL, 0, NULL, 0, NULL, NULL, NULL,
                     &fixture->wuss);
}

void wuss_fixture_close(wuss_fixture_t *fixture)
{
  if (fixture->wuss != NULL)
    wuss_destroy(fixture->wuss);
  fixture->wuss = NULL;

  forget_test_tasks();

  free(fixture->pixels);
  fixture->pixels = NULL;
}

/* ----------------------------------------------------------------------- */

/* A redraw handler that paints its content as one-pixel horizontal lines
 * whose colour encodes the document-space Y of each row, so a test can read
 * the framebuffer back and tell not just whose pixels are where but whether
 * a blit slid them by the right amount. task_data points to a colour_t used
 * only as a base hue (its blue channel is replaced per row). NULL task_data
 * means flood with a single fixed colour instead. */
result_t paint_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  const box_t *clip;
  const box_t *bounds;
  point_t      scroll;
  int          y, doc_y;

  NOT_USED(window);

  if (event->kind != wuss_EVENT_REDRAW)
    return result_OK;

  clip   = event->data.redraw.content;
  bounds = event->data.redraw.bounds;
  scroll = event->data.redraw.scroll;

  if (task_data == NULL)
  {
    screen_fill_rect(event->data.redraw.scr, clip->x0, clip->y0,
                     SIZE2D(clip->x1 - clip->x0, clip->y1 - clip->y0),
                     colour_rgb(0xcc, 0xdd, 0xee));
    return result_OK;
  }

  for (y = clip->y0; y < clip->y1; y++)
  {
    doc_y = y - bounds->y0 + scroll.y;
    screen_fill_rect(event->data.redraw.scr, clip->x0, y,
                     SIZE2D(clip->x1 - clip->x0, 1),
                     colour_rgb(0x20, 0x40, doc_y & 0xff));
  }

  return result_OK;
}
