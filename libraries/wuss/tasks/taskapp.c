/* wuss/tasks/taskapp.c -- icon bar app wrapper for the demo tasks */

#ifdef WUSS_APP

#include <ctype.h>
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
 * by calling its X_create (or opens its new-instance dialogue, if it has
 * one); MENU opens the app menu, whose "Info" row shows the program
 * information dialogue, whose optional "New..." row opens that dialogue and
 * whose "Hide" row hides every window of the app until it is "Show"n again
 * (or the icon is used to open another instance) and whose "Quit" row removes the icon and shuts down every instance. Each
 * instance is a task of its own, parented to the app task
 * (wuss_set_task_parent) so Quit can find it. */

/* "New..." is present only for an app with a new_dialogue, so Quit's row
 * index varies: see app_t::new_row and quit_row. Info is always first */
enum
{
  APP_MENU_INFO,
  APP_MENU_MAX = 6
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
  wuss_menu_item_t      menu_items[APP_MENU_MAX];
  wuss_menu_t           menu;
  int                   new_row;  /* menu row of "New...", or -1 */
  int                   create_row; /* menu rows of the create and adjust */
  int                   adjust_row; /* entries, or -1 when no adjust */
  int                   hide_row; /* menu row of "Hide" / "Show" */
  int                   quit_row; /* menu row of "Quit" */
  bool                  hidden; /* every window hidden by wuss_task_set_hidden */
  bool                  quit; /* Quit picked; taskapp_reap destroys the task */
};

static app_t *g_apps; /* every installed app */

static void app_label_hide(app_t *app, bool hidden)
{
  app->hidden = hidden;

  app->menu_items[app->hide_row].text = hidden ? "Show" : "Hide";
  wuss_menu_set_item_text_live(app->menu_handle, &app->menu, app->hide_row,
                               app->menu_items[app->hide_row].text);
}

/* whether the app is hidden, resynchronising the flag and the menu row with
 * the window manager: the hide keys hide an app without telling us */
static bool app_hidden(app_t *app)
{
  bool hidden;

  hidden = wuss_task_is_hidden(app->wuss, app->task) != 0;
  if (hidden != app->hidden)
    app_label_hide(app, hidden);

  return app->hidden;
}

static result_t app_set_hidden(app_t *app, bool hidden)
{
  app_label_hide(app, hidden);

  return wuss_task_set_hidden(app->wuss, app->task, hidden);
}

static result_t app_spawn(app_t             *app,
                          taskapp_spawn_fn_t fn,
                          void              *param,
                          void             **out)
{
  result_t rc;

  if (app_hidden(app))
    (void) app_set_hidden(app, false);

  wuss_set_task_parent(app->wuss, app->task);
  rc = fn(app->wuss, param, out);
  wuss_set_task_parent(app->wuss, NULL);

  if (rc != result_OK)
    logf_error("wuss: %s create failed, rc=0x%X (%s)", app->desc->name, rc,
               result_string(rc));
  return rc;
}

/* adapts desc->create to taskapp_spawn_fn_t; param is the app */
static result_t app_create_adapter(wuss_t *wuss, void *param, void **out)
{
  const app_t *app;

  app = param;

  return app->desc->create(wuss, out);
}

static result_t app_spawn_instance(app_t *app, void **out)
{
  return app_spawn(app, app_create_adapter, app, out);
}

static result_t app_adjust_adapter(wuss_t *wuss, void *param, void **out)
{
  const app_t *app;

  app = param;

  return app->desc->adjust(wuss, out);
}

/* what SELECT on the icon and the "New..." row do */
static result_t app_new(app_t *app)
{
  result_t rc;

  if (app_hidden(app))
    (void) app_set_hidden(app, false);

  if (app->desc->singleton &&
      wuss_task_raise_children(app->wuss, app->task))
    return result_OK;

  if (app->desc->new_dialogue == NULL)
    return app_spawn_instance(app, NULL);

  /* a task the dialogue makes belongs to the app too, and goes with it */
  wuss_set_task_parent(app->wuss, app->task);
  rc = app->desc->new_dialogue(app->wuss);
  wuss_set_task_parent(app->wuss, NULL);

  return rc;
}

result_t taskapp_spawn(wuss_t               *wuss,
                       const taskapp_desc_t *desc,
                       taskapp_spawn_fn_t    fn,
                       void                 *param,
                       void                **out)
{
  app_t *app;

  for (app = g_apps; app != NULL; app = app->next)
    if (app->desc == desc)
      return app_spawn(app, fn, param, out);

  (void) wuss;

  return result_NOT_FOUND;
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

/* A file double-clicked in a Filer: claim it if its type is in desc->opens,
 * then open an instance and load it. Claimed even if the load fails: the app
 * reports its own load errors, so the Filer must not report another. */
static result_t app_data_open(app_t *app, const wuss_message_t *msg)
{
  const wuss_data_open_t *open;
  const unsigned short   *type;
  void                   *instance;

  if (app->desc->opens == NULL || app->desc->data_load == NULL ||
      msg->size < sizeof(*open))
    return result_OK;

  open = (const wuss_data_open_t *) msg->data;
  for (type = app->desc->opens; *type != 0; type++)
    if (*type == open->filetype.riscos)
      break;
  if (*type == 0)
    return result_OK;

  (void) wuss_acknowledge(app->wuss, app->task, msg);

  if (app_spawn_instance(app, &instance) == result_OK)
    (void) app->desc->data_load(instance, open->path);

  return result_OK;
}

result_t taskapp_window_data_load(wuss_t                *wuss,
                                  wuss_task_t           *delegate,
                                  void                  *instance,
                                  taskapp_data_load_fn_t load,
                                  const wuss_message_t  *msg)
{
  char   path[DPTLIB_MAXPATH];
  size_t len;

  if (msg->action != wuss_MESSAGE_DATA_LOAD)
    return result_OK;

  len = MIN(msg->size, sizeof(path) - 1);
  memcpy(path, msg->data, len);
  path[len] = '\0';

  if (load(instance, path) == result_OK)
    (void) wuss_acknowledge(wuss, delegate, msg);

  return result_OK;
}

static result_t app_menu_open(app_t *app)
{
  const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC(app->desc->name, app->desc->purpose);

  (void) app_hidden(app);
  wuss_proginfo_set_desc(&desc);
  app->menu_items[APP_MENU_INFO].window = wuss_proginfo_window(app->task);
  if (wuss_task_has_windows(app->wuss, app->task))
    app->menu_items[app->hide_row].flags &=
      (wuss_menu_item_flags_t) ~wuss_MENU_ITEM_DISABLED;
  else
    app->menu_items[app->hide_row].flags |= wuss_MENU_ITEM_DISABLED;

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
    if (app_hidden(app)) /* the click only reveals; it opens nothing new */
      return app_set_hidden(app, false);
    if (event->data.iconbar_icon.button & wuss_BUTTON_SELECT)
      return app->desc->drop_only ? result_OK : app_new(app);
    if ((event->data.iconbar_icon.button & wuss_BUTTON_ADJUST) &&
        app->desc->adjust != NULL)
      return app_spawn(app, app_adjust_adapter, app, NULL);
    return result_OK;

  case wuss_EVENT_MESSAGE:
    if (window != NULL)
      return result_OK;
    if (event->data.message->action == wuss_MESSAGE_DATA_OPEN)
      return app_data_open(app, event->data.message);
    return app_data_load(app, event->data.message);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu != &app->menu)
      return result_OK;
    if (event->data.menu_select.index == app->quit_row)
      app->quit = true;
    else if (event->data.menu_select.index == app->hide_row)
      return app_set_hidden(app, !app_hidden(app));
    else if (event->data.menu_select.index == app->new_row)
      return app_new(app);
    else if (event->data.menu_select.index == app->create_row)
      return app_spawn_instance(app, NULL);
    else if (event->data.menu_select.index == app->adjust_row)
      return app_spawn(app, app_adjust_adapter, app, NULL);
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

void taskapp_glyph_name(const char *name, char leaf[TASKAPP_GLYPH_NAME_MAX])
{
  size_t n;

  memcpy(leaf, "app_", 4);
  n = 4;
  for (; *name != '\0' && n < TASKAPP_GLYPH_NAME_MAX - 1; name++)
    if (isalnum((unsigned char) *name))
      leaf[n++] = (char) tolower((unsigned char) *name);
  leaf[n] = '\0';
}

const bitmap_t *taskapp_glyph(const wuss_task_t *task, const char *name)
{
  char            leaf[TASKAPP_GLYPH_NAME_MAX];
  const bitmap_t *bm;

  taskapp_glyph_name(name, leaf);
  bm = wuss_task_iconpool_find(task, leaf);
  if (bm == NULL)
    bm = wuss_task_iconpool_find(task, "file_app");
  return bm;
}

result_t taskapp_install(wuss_t *wuss, const taskapp_desc_t *desc)
{
  result_t                 rc;
  app_t                   *app;
  wuss_task_desc_t         task_desc;
  wuss_iconbar_icon_spec_t spec;
  int                      n;

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
  n = APP_MENU_INFO + 1;
  app->new_row = -1;
  if (desc->new_dialogue != NULL)
  {
    app->new_row = n;
    WUSS_MENU_ITEM(app->menu_items, n, "New...", wuss_MENU_ITEM_NONE);
    n++;
  }
  app->create_row = -1;
  app->adjust_row = -1;
  if (desc->adjust != NULL)
  {
    app->create_row = n;
    WUSS_MENU_ITEM(app->menu_items, n, desc->create_label,
                   wuss_MENU_ITEM_NONE);
    n++;
    app->adjust_row = n;
    WUSS_MENU_ITEM(app->menu_items, n, desc->adjust_label,
                   wuss_MENU_ITEM_NONE);
    n++;
  }
  app->hide_row = n;
  WUSS_MENU_ITEM_SHORTCUT(app->menu_items, n, "Hide", wuss_MENU_ITEM_NONE,
                          "^H");
  n++;
  app->quit_row = n;
  WUSS_MENU_ITEM_SHORTCUT(app->menu_items, n, "Quit", 0, NULL);
  n++;
  WUSS_MENU_TITLE(app->menu, desc->name, app->menu_items, n);

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

  spec.image = taskapp_glyph(app->task, desc->name);
  spec.text  = desc->name;
  if (spec.image == NULL)
    logf_warning("taskapp: icon 'file_app' unavailable; icons show label only");
  else
    spec.text = "";
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

result_t taskapp_spawn(wuss_t               *wuss,
                       const taskapp_desc_t *desc,
                       taskapp_spawn_fn_t    fn,
                       void                 *param,
                       void                **out)
{
  (void) desc;

  return fn(wuss, param, out);
}

void taskapp_reap(void)
{
}

#endif /* WUSS_ICONBAR */

#endif /* WUSS_APP */
