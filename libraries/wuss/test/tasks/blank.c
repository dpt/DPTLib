/* wuss/test/tasks/blank.c -- colour-cycling task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "blank.h"

#define BLANK_CYCLE_FRAMES 30 /* colour advances every half-second at 60fps */

/* MENU click pops this single-item menu; the item table and wuss_menu_t
 * live per-instance in blank_task_t, not as a file-scope static, so that
 * each window's Info row can hold its own .window pointer to the shared
 * proginfo singleton, retargeted just before wuss_menu_open */
enum { BLANK_MENU_INFO };

result_t blank_create(wuss_t *wuss, blank_task_t **out)
{
  result_t         rc;
  blank_task_t    *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->npalette    = 16; // TODO: Read max palette index from wuss
  task->index       = 0;
  task->frame_count = 0;

  /* wuss fills the content area itself */
  delegate_desc.handle    = blank_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "blank";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(200, 160),
                                 NULL,
                                 wuss_WINDOW_CLOSE | wuss_WINDOW_VSCROLL | wuss_WINDOW_HSCROLL |
                                 wuss_WINDOW_RESIZE,
                                 wuss_BACKDROP_COLOUR(task->index),
                                 SIZE2D(200, 160),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, BLANK_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * blank_handle */

  WUSS_MENU_TITLE(task->menu, "Blank", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void blank_destroy(blank_task_t *task)
{
  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t blank_idle(void *task_data)
{
  result_t      rc;
  blank_task_t *bc;

  bc = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (bc->window == NULL)
    return result_OK;

  if (++bc->frame_count < BLANK_CYCLE_FRAMES)
    return result_OK;

  bc->frame_count = 0;
  bc->index       = (bc->index + 1) % bc->npalette;

  rc = wuss_window_set_background(bc->window,
                                  wuss_BACKDROP_COLOUR(bc->index));
  if (rc != result_OK)
    logf_warning("blank_idle: wuss_window_set_background(%d) failed", bc->index);

  return rc;
}

result_t blank_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  blank_task_t *bc;

  bc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_IDLE:
    return blank_idle(task_data);

  case wuss_EVENT_MOUSE:
    if (window != bc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_MENU))
      return result_OK;
    {
      static const wuss_proginfo_desc_t desc =
      {
        "Blank",
        "Colour-cycling backdrop, no redraw callback",
        "(c) DPTLib contributors",
        "1.0 (" __DATE__ ")"
      };
      wuss_proginfo_set_desc(&desc);
      bc->menu_items[BLANK_MENU_INFO].window =
        wuss_proginfo_window(bc->delegate);
    }
    return wuss_menu_open(bc->delegate, &bc->menu,
                          wuss_get_pointer(bc->wuss), &bc->menu_handle);

  case wuss_EVENT_MENU_CLOSED:
    bc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == bc->window)
      bc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == bc->menu_items[BLANK_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_QUIT:
    blank_destroy(bc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
