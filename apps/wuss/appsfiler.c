/* appsfiler.c -- permanent icon bar "Apps" folder listing the launchable tasks */

#ifdef WUSS_ICONBAR

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "io/path.h"
#include "wuss/gadget/gridview.h"
#include "wuss/component/proginfo.h"
#include "wuss/iconbar.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "appsfiler.h"
#include "tasks.h"
#include "tasks/common.h"

#define APPS_LABEL_CHARS 12
#define APPS_COLUMNS     4
#define APPS_MAX_ROWS    4
#define APPS_MENU_GAP    47 /* icon bar menu bottom to screen bottom, pixels */

/* ponytail: window slots are inline, 0 the root plus one per category, so
 * there is no malloc bookkeeping. Raise it if more categories are added. */
#define APPS_MAX_CATEGORIES 15

typedef struct apps_task apps_task_t;

typedef struct
{
  apps_task_t     *owner;
  int              category; /* -1: the root window of category folders */
  wuss_window_t   *window;   /* NULL when closed */
  wuss_gridview_t *gridview;
}
apps_window_t;

struct apps_task
{
  wuss_t               *wuss;
  wuss_task_t          *delegate;
  wuss_iconbar_icon_t  *iconbar_icon;

  apps_window_t         windows[1 + APPS_MAX_CATEGORIES]; /* [0] root,
                                                           * [c + 1] category c */
  bitmap_t              glyph_file;
  bitmap_t              glyph_directory;
  int                   have_glyph_file;
  int                   have_glyph_directory;

  wuss_menu_handle_t    menu_handle;
  wuss_menu_item_t      menu_items[1]; /* "Info" */
  wuss_menu_t           menu;
};

static int apps_count(const apps_window_t *aw)
{
  if (aw->category < 0)
    return MIN(tasks_category_count(), APPS_MAX_CATEGORIES);

  return tasks_entry_count(aw->category);
}

static void apps_item(int index, void *opaque, wuss_gridview_item_t *out)
{
  apps_window_t *aw;

  aw = opaque;

  if (aw->category < 0)
  {
    out->label = tasks_category_name(index);
    out->glyph = aw->owner->have_glyph_directory ?
                 &aw->owner->glyph_directory : NULL;
    return;
  }

  out->label = tasks_entry_name(aw->category, index);
  out->glyph = aw->owner->have_glyph_file ? &aw->owner->glyph_file : NULL;
}

/* ----------------------------------------------------------------------- */

static void apps_activate(wuss_gridview_t *gridview,
                          int              index,
                          void            *opaque);

static result_t apps_open(apps_task_t *task, int category)
{
  result_t       rc;
  apps_window_t *aw;
  wuss_window_t *window;
  const char    *title;

  aw = &task->windows[category + 1];
  if (aw->window != NULL)
  {
    wuss_window_restack(aw->window, wuss_ZORDER_FRONT);
    return result_OK;
  }

  title = category < 0 ? "Apps" : tasks_category_name(category);

  rc = wuss_window_create_placed(task->delegate, SIZE2D(320, 240), title,
                                 (wuss_WINDOW_DEFAULT & ~wuss_WINDOW_HSCROLL) |
                                 wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP, SIZE2D(320, 240),
                                 SIZE2D(0, 0), &window);
  if (rc != result_OK)
    return rc;

  aw->owner    = task;
  aw->category = category;

  rc = wuss_gridview_create(&aw->gridview, window, apps_count(aw), apps_item,
                            APPS_LABEL_CHARS, apps_activate, aw);
  if (rc != result_OK)
  {
    wuss_window_close(window);
    return rc;
  }

  aw->window = window;

  (void) wuss_window_resize(window, wuss_gridview_natural_size(
                              aw->gridview, APPS_COLUMNS, APPS_MAX_ROWS));
  return result_OK;
}

static void apps_activate(wuss_gridview_t *gridview, int index, void *opaque)
{
  apps_window_t *aw;

  NOT_USED(gridview);

  aw = opaque;
  if (aw->category < 0)
    (void) apps_open(aw->owner, index);
  else
    (void) tasks_entry_spawn(aw->category, index);
}

/* ----------------------------------------------------------------------- */

static apps_window_t *apps_find(apps_task_t         *task,
                                const wuss_window_t *window)
{
  int i;

  for (i = 0; i < (int) NELEMS(task->windows); i++)
    if (task->windows[i].window == window && window != NULL)
      return &task->windows[i];

  return NULL;
}

static void apps_destroy(apps_task_t *task)
{
  int i;

  for (i = 0; i < (int) NELEMS(task->windows); i++)
    if (task->windows[i].gridview != NULL)
      wuss_gridview_destroy(task->windows[i].gridview);

  if (task->have_glyph_file)
    free(task->glyph_file.base);
  if (task->have_glyph_directory)
    free(task->glyph_directory.base);

  free(task);
}

static result_t apps_menu_open(apps_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("AppsFiler", "Launcher for the demo tasks");

  wuss_proginfo_set_desc(&desc);
  task->menu_items[0].window = wuss_proginfo_window(task->delegate);

  return wuss_menu_open_above(task->delegate, &task->menu,
                              wuss_get_pointer(task->wuss).x,
                              APPS_MENU_GAP, &task->menu_handle);
}

static result_t apps_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data)
{
  apps_task_t   *task;
  apps_window_t *aw;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
  case wuss_EVENT_MOUSE:
  case wuss_EVENT_OPEN:
  case wuss_EVENT_PALETTE:
  case wuss_EVENT_RELAYOUT:
    aw = apps_find(task, window);
    if (aw != NULL)
      (void) wuss_gridview_handle_event(aw->gridview, event);
    return result_OK;

  case wuss_EVENT_ICON:
    if (window != NULL || event->data.iconbar_icon.action != wuss_MOUSE_UP)
      return result_OK;
    if (event->data.iconbar_icon.button & wuss_BUTTON_MENU)
      return apps_menu_open(task);
    return apps_open(task, -1);

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->menu_items[0].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK || event->data.pre_show.handle == NULL)
      return rc;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_CLOSE:
    aw = apps_find(task, window);
    if (aw == NULL)
      return result_OK;

    wuss_gridview_destroy(aw->gridview);
    aw->gridview = NULL;
    aw->window   = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    apps_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

static void apps_load_glyph(const char *resources,
                            const char *leaf,
                            bitmap_t   *bm,
                            int        *have)
{
  const char *path;

  path  = pathf("%s/resources/wuss/files/%s.png", resources, leaf);
  *have = bitmap_load_png(bm, path) == result_OK;
  if (!*have)
    logf_warning("appsfiler: glyph '%s' unavailable; cells show label only",
                 path);
}

result_t appsfiler_create(wuss_t *wuss)
{
  result_t                 rc;
  apps_task_t             *task;
  wuss_task_desc_t         desc;
  wuss_iconbar_icon_spec_t spec;
  const char              *resources;
  int                      i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  for (i = 0; i < (int) NELEMS(task->windows); i++)
  {
    task->windows[i].owner    = task;
    task->windows[i].category = i - 1;
  }

  resources = wuss_get_resources(wuss);
  apps_load_glyph(resources, "file_unk", &task->glyph_file,
                  &task->have_glyph_file);
  apps_load_glyph(resources, "file_dir", &task->glyph_directory,
                  &task->have_glyph_directory);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, 0, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo
                                * singleton in apps_menu_open */
  WUSS_MENU_TITLE(task->menu, "Apps", task->menu_items,
                  NELEMS(task->menu_items));

  desc.handle    = apps_handle;
  desc.task_data = task;
  desc.name      = "appsfiler";
  rc = wuss_task_create(wuss, &desc, &task->delegate);
  if (rc != result_OK)
  {
    apps_destroy(task);
    return rc;
  }

  spec.text  = "Apps";
  spec.image = task->have_glyph_directory ? &task->glyph_directory : NULL;
  rc = wuss_iconbar_icon_create(wuss, task->delegate, &spec,
                                &task->iconbar_icon);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* its QUIT frees the task block */
    return rc;
  }

  return result_OK;
}

#endif /* WUSS_ICONBAR */
