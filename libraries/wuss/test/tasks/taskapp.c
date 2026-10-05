/* wuss/test/tasks/taskapp.c -- icon bar app wrapper for the demo tasks */

#ifdef WUSS_APP

#include <stdbool.h>
#include <stdlib.h>

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

/* The application glyph is shared by every app's icon: loaded for the first
 * and freed with the last. */
static bitmap_t g_glyph;
static bool     g_glyph_loaded;
static int      g_glyph_refs;

static const bitmap_t *glyph_acquire(const wuss_t *wuss)
{
  const char *path;

  if (g_glyph_refs == 0)
  {
    path           = pathf("%s/resources/wuss/files/file_app.png",
                           wuss_get_resources(wuss));
    g_glyph_loaded = bitmap_load_png(&g_glyph, path) == result_OK;
    if (!g_glyph_loaded)
      logf_warning("taskapp: glyph '%s' unavailable; icons show label only",
                   path);
  }
  g_glyph_refs++;

  return g_glyph_loaded ? &g_glyph : NULL;
}

static void glyph_release(void)
{
  if (--g_glyph_refs > 0)
    return;

  if (g_glyph_loaded)
    free(g_glyph.base);
  g_glyph_loaded = false;
}

static result_t app_spawn_instance(app_t *app)
{
  result_t rc;

  wuss_set_task_parent(app->wuss, app->task);
  rc = app->desc->create(app->wuss, NULL);
  wuss_set_task_parent(app->wuss, NULL);

  if (rc != result_OK)
    logf_error("wuss: %s create failed, rc=0x%X (%s)", app->desc->name, rc,
               result_string(rc));
  return rc;
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
      return app_spawn_instance(app);
    return result_OK; /* ADJUST: nothing yet */

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
    glyph_release();
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
  spec.image = glyph_acquire(wuss);
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
