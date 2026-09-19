/* wuss/test/tasks/display.h -- desktop resolution picker task */

#ifndef TASKS_DISPLAY_H
#define TASKS_DISPLAY_H

#ifdef WUSS_APP

#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

/* how many fixed resolutions the picker offers; see display.c's
 * g_display_resolutions */
#define DISPLAY_MAX_RESOLUTIONS 8

/* window D's task: a small window reporting the current screen size. A MENU
 * click opens a picker of fixed resolutions; a pick calls app_resize, which
 * live-resizes the frontend's surface and wuss's own screen, nudging every
 * open window back on-screen and shrinking any that no longer fit. */
typedef struct display_task
{
  wuss_t             *wuss;
  wuss_task_t        *delegate;
  wuss_window_t      *window;
  wuss_menu_handle_t  menu_handle;
  wuss_menu_item_t    menu_items[1 + DISPLAY_MAX_RESOLUTIONS]; /* Info, then
                                    * one row per g_display_resolutions
                                    * entry */
  wuss_menu_t         menu;
}
display_task_t;

wuss_window_fn_t display_handle;

/* create the resolution-picker window against the given wuss instance. if
 * out is non-NULL, the task block is also returned through it */
result_t display_create(wuss_t *wuss, display_task_t **out);

/* free a task block allocated by display_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void display_destroy(display_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_DISPLAY_H */
