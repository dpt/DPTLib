/* wuss/test/tasks/taskapp.c -- icon bar app wrapper for the demo tasks */

#ifdef WUSS_APP

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "io/path.h"
#include "wuss/component/proginfo.h"
#include "wuss/iconbar.h"
#include "wuss/menu.h"
#include "wuss/message.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "common.h"
#include "taskapp.h"

#ifdef WUSS_ICONBAR

/* ----- icon bar apps ----------------------------------------------------
 *
 * SELECT on an app's icon opens another instance of the task's main window
 * by calling its X_create; MENU opens the app menu, whose "Info" row shows
 * the program information dialogue and whose "Quit" row removes the icon and
 * shuts down every instance. Each instance is a task of its own, parented to
 * the app task (wuss_set_task_parent) so Quit can find it. */

enum
{
  APP_MENU_INFO,
  APP_MENU_QUIT,
  APP_MENU_COUNT
};

typedef struct app app_t;

struct app
{
  app_t                *next; /* g_apps */
  wuss_t               *wuss;
  const taskapp_desc_t *desc;
  wuss_task_t          *task;
  wuss_iconbar_icon_t  *icon;
  wuss_menu_handle_t    menu_handle;
  wuss_menu_item_t      menu_items[APP_MENU_COUNT];
  wuss_menu_t           menu;
  bool                  quit; /* Quit picked; taskapp_reap destroys the task */
};

static app_t *g_apps; /* every installed app */

static result_t app_spawn_instance(app_t *app, void **out)
{
  result_t rc;

  wuss_set_task_parent(app->wuss, app->task);
  rc = app->desc->create(app->wuss, out);
  wuss_set_task_parent(app->wuss, NULL);

  if (rc != result_OK)
    logf_error("wuss: %s create failed, rc=0x%X (%s)", app->desc->name, rc,
               result_string(rc));
  return rc;
}

/* A file dropped on the icon: open an instance and load it. An instance that
 * will not load the file stays open, empty, and the message bounces. */
static result_t app_data_load(app_t *app, const wuss_message_t *msg)
{
  result_t rc;
  void    *instance;
  char     path[DPTLIB_MAXPATH];
  size_t   len;

  if (msg->action != wuss_MESSAGE_DATA_LOAD || app->desc->data_load == NULL)
    return result_OK;

  len = MIN(msg->size, sizeof(path) - 1);
  memcpy(path, msg->data, len);
  path[len] = '\0';

  rc = app_spawn_instance(app, &instance);
  if (rc != result_OK)
    return result_OK;

  if (app->desc->data_load(instance, path) == result_OK)
    (void) wuss_acknowledge(app->wuss, app->task, msg);

  return result_OK;
}

static result_t app_menu_open(app_t *app)
{
  const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC(app->desc->name, app->desc->purpose);

  wuss_proginfo_set_desc(&desc);
  app->menu_items[APP_MENU_INFO].window = wuss_proginfo_window(app->task);

  return wuss_iconbar_menu_open(app->task, &app->menu, &app->menu_handle);
}

static result_t app_handle(wuss_window_t      *window,
                           const wuss_event_t *event,
                           void               *task_data)
{
  app_t  *app;
  app_t **link;

  app = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    if (window != NULL || event->data.iconbar_icon.action != wuss_MOUSE_UP)
      return result_OK;
    if (event->data.iconbar_icon.button & wuss_BUTTON_MENU)
      return app_menu_open(app);
    if (event->data.iconbar_icon.button & wuss_BUTTON_SELECT)
      return app_spawn_instance(app, NULL);
    return result_OK; /* ADJUST: nothing yet */

  case wuss_EVENT_MESSAGE:
    if (window == NULL)
      return app_data_load(app, event->data.message);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &app->menu &&
        event->data.menu_select.index == APP_MENU_QUIT)
      app->quit = true;
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    app->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == app->menu_items[APP_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK || event->data.pre_show.handle == NULL)
      return rc;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_QUIT:
    wuss_proginfo_release(app->task);
    for (link = &g_apps; *link != app; link = &(*link)->next)
      ;
    *link = app->next;
    free(app);
    return result_OK;

  default:
    return result_OK;
  }
}

result_t taskapp_install(wuss_t *wuss, const taskapp_desc_t *desc)
{
  result_t                 rc;
  app_t                   *app;
  wuss_task_desc_t         task_desc;
  wuss_iconbar_icon_spec_t spec;

  for (app = g_apps; app != NULL; app = app->next)
    if (app->desc == desc)
      return result_OK;

  app = calloc(1, sizeof(*app));
  if (app == NULL)
    return result_OOM;

  app->wuss = wuss;
  app->desc = desc;

  WUSS_MENU_ITEM_WINDOW(app->menu_items, APP_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo
                                * singleton in app_menu_open */
  WUSS_MENU_ITEM_SHORTCUT(app->menu_items, APP_MENU_QUIT, "Quit", 0, NULL);
  WUSS_MENU_TITLE(app->menu, desc->name, app->menu_items,
                  NELEMS(app->menu_items));

  task_desc.handle    = app_handle;
  task_desc.task_data = app;
  task_desc.name      = desc->name;
  rc = wuss_task_create(wuss, &task_desc, &app->task);
  if (rc != result_OK)
  {
    free(app);
    return rc;
  }
  app->next = g_apps;
  g_apps    = app;

  spec.text  = desc->name;
  spec.image = wuss_task_iconpool_find(app->task, "file_app");
  if (spec.image == NULL)
    logf_warning("taskapp: icon 'file_app' unavailable; icons show label only");
  spec.side  = wuss_ICONBAR_RIGHT;
  rc = wuss_iconbar_icon_create(wuss, app->task, &spec, &app->icon);
  if (rc != result_OK)
  {
    wuss_task_destroy(app->task); /* its QUIT unlinks and frees app */
    logf_error("wuss: %s install failed, rc=0x%X (%s)", desc->name, rc,
               result_string(rc));
  }

  return rc;
}

void taskapp_reap(void)
{
  app_t *app, *next;

  for (app = g_apps; app != NULL; app = next)
  {
    next = app->next;
    if (app->quit)
      wuss_task_destroy(app->task); /* its QUIT frees app */
  }
}

#else /* !WUSS_ICONBAR */

result_t taskapp_install(wuss_t *wuss, const taskapp_desc_t *desc)
{
  return desc->create(wuss, NULL);
}

void taskapp_reap(void)
{
}

#endif /* WUSS_ICONBAR */

#endif /* WUSS_APP */
