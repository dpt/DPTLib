/* wuss/test/tasks/config.h -- startup settings task (mouse button swap) */

#ifndef TASKS_CONFIG_H
#define TASKS_CONFIG_H

#ifdef WUSS_APP

#include "wuss/component/proginfo.h"
#include "wuss/icon.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* Startup settings window: currently a single option icon toggling
 * right/middle mouse button swap (g_tasks.swap_mouse_buttons, read by each
 * frontend's raw-button translator). Opened once from run_wuss, not from the
 * task launcher menu. */
typedef struct config_task
{
  wuss_t             *wuss;     /* for wuss_get_pointer when opening the
                                 * menu */
  wuss_task_t        *delegate; /* the task that owns the menu */
  wuss_window_t      *window;
  wuss_icon_t         *swap_icon;
  wuss_menu_handle_t  menu_handle; /* live only between open and a pick */
  wuss_menu_item_t    menu_items[1]; /* per-instance: a shared static would
                                       * leak one instance's Info row .window
                                       * into another's menu */
  wuss_menu_t         menu;
}
config_task_t;

wuss_window_fn_t config_handle;

/* create the startup settings window against the given wuss instance; if out
 * is non-NULL, the task block is also returned through it */
result_t config_create(wuss_t *wuss, config_task_t **out);

/* free a task block allocated by config_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void config_destroy(config_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CONFIG_H */
