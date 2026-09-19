/* wuss/test/tasks/config.c -- startup settings task (mouse button swap,
 * reverse scroll) */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "tasks.h" /* g_tasks.swap_mouse_buttons */

#include "config.h"

#define CONFIG_MARGIN 8
#define CONFIG_DOC_W  220
#define CONFIG_ROW    20
#define CONFIG_DOC_H  (CONFIG_MARGIN * 2 + CONFIG_ROW + 16)

/* MENU click pops this single-item menu; the item table and wuss_menu_t live
 * per-instance in config_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum { CONFIG_MENU_INFO };

result_t config_create(wuss_t *wuss, config_task_t **out)
{
  result_t         rc;
  config_task_t   *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  wuss_icon_spec_t spec;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  delegate_desc.handle    = config_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "config";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(CONFIG_DOC_W, CONFIG_DOC_H),
                                 "Configure",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 SIZE2D(CONFIG_DOC_W, CONFIG_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  memset(&spec, 0, sizeof(spec));
  spec.bbox = (box_t) BOX_POS_SIZE(CONFIG_MARGIN, CONFIG_MARGIN,
                                   CONFIG_DOC_W - 2 * CONFIG_MARGIN, 16);
  spec.type = wuss_ICON_TYPE_OPTION;
  spec.text = "Swap right/middle mouse buttons";
  spec.fg   = wuss_COLOUR_BLACK;
  spec.bg   = wuss_NO_BACKGROUND;

  rc = wuss_icon_create(task->window, &spec, &task->swap_icon);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes the window; QUIT frees task block */
    return rc;
  }
  wuss_icon_set_selected(task->window, task->swap_icon,
                         g_tasks.swap_mouse_buttons);

  memset(&spec, 0, sizeof(spec));
  spec.bbox = (box_t) BOX_POS_SIZE(CONFIG_MARGIN, CONFIG_MARGIN + CONFIG_ROW,
                                   CONFIG_DOC_W - 2 * CONFIG_MARGIN, 16);
  spec.type = wuss_ICON_TYPE_OPTION;
  spec.text = "Reverse mouse scroll direction";
  spec.fg   = wuss_COLOUR_BLACK;
  spec.bg   = wuss_NO_BACKGROUND;

  rc = wuss_icon_create(task->window, &spec, &task->reverse_scroll_icon);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes the window; QUIT frees task block */
    return rc;
  }
  wuss_icon_set_selected(task->window, task->reverse_scroll_icon,
                         g_tasks.reverse_scroll);

  /* fully built: from here a last-window close reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, CONFIG_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * config_handle */

  WUSS_MENU_TITLE(task->menu, "Configure", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void config_destroy(config_task_t *task)
{
  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t config_icon(const wuss_event_t *event, void *task_data)
{
  config_task_t *cc;
  wuss_icon_t   *icon;

  cc   = task_data;
  icon = event->data.icon.icon;

  if (event->data.icon.action != wuss_MOUSE_UP)
    return result_OK;

  if (icon == cc->swap_icon)
    g_tasks.swap_mouse_buttons = wuss_icon_get_selected(icon) ? true : false;
  else if (icon == cc->reverse_scroll_icon)
    g_tasks.reverse_scroll = wuss_icon_get_selected(icon) ? true : false;

  return result_OK;
}

result_t config_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  config_task_t *cc;

  cc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    return config_icon(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != cc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN ||
        !(event->data.mouse.button & wuss_BUTTON_MENU))
      return result_OK;
    {
      static const wuss_proginfo_desc_t desc =
      {
        "Configure",
        "Startup settings: buttons, scroll",
        "(c) DPTLib contributors",
        "1.0 (" __DATE__ ")"
      };
      wuss_proginfo_set_desc(&desc);
      cc->menu_items[CONFIG_MENU_INFO].window =
        wuss_proginfo_window(cc->delegate);
    }
    return wuss_menu_open(cc->delegate, &cc->menu,
                          wuss_get_pointer(cc->wuss), &cc->menu_handle);

  case wuss_EVENT_MENU_CLOSED:
    cc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == cc->window)
      cc->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == cc->menu_items[CONFIG_MENU_INFO].window)
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
    config_destroy(cc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
