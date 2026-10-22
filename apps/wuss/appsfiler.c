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
#include "wuss/gadget/gridview.h"
#include "wuss/component/proginfo.h"
#include "wuss/iconbar.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "appsfiler.h"
#include "tasks.h"
#include "tasks/common/common.h"
#include "tasks/common/taskapp.h"

#define APPS_LABEL_CHARS 12
#define APPS_COLUMNS     4
#define APPS_MAX_ROWS    4

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

enum
{
  APPS_MENU_INFO,
  APPS_MENU_CLOSE_ALL,
  APPS_MENU_HIDE,
  APPS_MENU__LIMIT
};

struct apps_task
{
  wuss_t               *wuss;
  wuss_task_t          *delegate;
  wuss_iconbar_icon_t  *iconbar_icon;

  apps_window_t         windows[1 + APPS_MAX_CATEGORIES]; /* [0] root,
                                                           * [c + 1] category c */
  const bitmap_t       *glyph_directory; /* borrowed from the icon pool */
  const bitmap_t       *glyph_directory_open; /* for a category with its
                                               * window open */
  const bitmap_t       *glyph_apps;

  wuss_menu_handle_t    menu_handle;
  wuss_menu_item_t      menu_items[APPS_MENU__LIMIT];
  wuss_menu_t           menu;
  int                   hidden; /* windows hidden by Hide */
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
    out->glyph = aw->owner->glyph_directory;
    if (aw->owner->windows[index + 1].window != NULL &&
        aw->owner->glyph_directory_open != NULL)
      out->glyph = aw->owner->glyph_directory_open;
    return;
  }

  out->label = tasks_entry_name(aw->category, index);
  out->glyph = taskapp_glyph(aw->owner->delegate, out->label);
}

/* ----------------------------------------------------------------------- */

static void apps_activate(wuss_gridview_t *gridview,
                          int              index,
                          wuss_button_t    button,
                          void            *opaque);

/* Opens the window for category (-1: the root), or brings it to the front
 * if open. A new window cascades from parent, or with over set takes
 * parent's place (top-left) for a parent that is about to close. */
static void apps_label_hide(apps_task_t *task, int hidden)
{
  wuss_menu_item_t *item;

  task->hidden = hidden;

  item       = &task->menu_items[APPS_MENU_HIDE];
  item->text = hidden ? "Show" : "Hide";
  wuss_menu_set_item_text_live(task->menu_handle, &task->menu, APPS_MENU_HIDE,
                               item->text);
}

/* whether the windows are hidden, resynchronising the flag and the menu row
 * with the window manager: the hide keys hide a task without telling us */
static int apps_hidden(apps_task_t *task)
{
  int hidden;

  hidden = wuss_task_is_hidden(task->wuss, task->delegate) != 0;
  if (hidden != task->hidden)
    apps_label_hide(task, hidden);

  return task->hidden;
}

/* hide or show every window, relabelling the icon bar menu's row to match */
static result_t apps_set_hidden(apps_task_t *task, int hidden)
{
  apps_label_hide(task, hidden);

  return wuss_task_set_hidden(task->wuss, task->delegate, hidden);
}

static result_t apps_open(apps_task_t         *task,
                          int                  category,
                          const wuss_window_t *parent,
                          int                  over)
{
  result_t       rc;
  apps_window_t *aw;
  const char    *title;
  wuss_place_t   place;
  wuss_window_t *window;

  if (apps_hidden(task))
    (void) apps_set_hidden(task, 0);

  aw = &task->windows[category + 1];
  if (aw->window != NULL)
  {
    wuss_window_restack(aw->window, wuss_ZORDER_FRONT);
    return result_OK;
  }

  title = category < 0 ? "Apps" : tasks_category_name(category);

  aw->owner    = task;
  aw->category = category;

  place = task_place_from(parent);
  if (over)
    place.offset = POINT(0, 0);

  rc = wuss_gridview_window_create(&aw->gridview, task->delegate, &place,
                                   title, apps_count(aw), apps_item,
                                   APPS_LABEL_CHARS, apps_activate, aw,
                                   0, &window);
  if (rc != result_OK)
    return rc;

  aw->window = window;

  wuss_gridview_fit_window(aw->gridview, APPS_COLUMNS, APPS_MAX_ROWS, &place);

  /* the root's folder for this category now shows open */
  wuss_window_invalidate_visible(task->windows[0].window);
  return result_OK;
}

static void apps_activate(wuss_gridview_t *gridview,
                          int              index,
                          wuss_button_t    button,
                          void            *opaque)
{
  apps_window_t *aw;

  NOT_USED(gridview);

  aw = opaque;
  if (aw->category >= 0)
  {
    /* as RISC OS: a task launched with a double ADJUST closes its viewer.
     * That destroys gridview; neither it nor aw's window is used after. */
    if (tasks_entry_spawn(aw->category, index) == result_OK &&
        (button & wuss_BUTTON_ADJUST))
      (void) wuss_window_try_close(aw->window);
    return;
  }

  /* as the filer: a double ADJUST opens the category in the root's place and
   * closes the root. That destroys gridview; neither it nor aw's window is
   * used after. */
  (void) apps_open(aw->owner, index, aw->window, button & wuss_BUTTON_ADJUST);

  if (button & wuss_BUTTON_ADJUST)
    (void) wuss_window_try_close(aw->window);
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

  free(task);
}

static result_t apps_menu_open(apps_task_t *task)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("AppsFiler", "Launcher for demo apps");

  (void) apps_hidden(task);
  wuss_proginfo_set_desc(&desc);
  task->menu_items[APPS_MENU_INFO].window = wuss_proginfo_window(task->delegate);

  if (wuss_task_has_windows(task->wuss, task->delegate))
  {
    task->menu_items[APPS_MENU_CLOSE_ALL].flags &=
      (wuss_menu_item_flags_t) ~wuss_MENU_ITEM_DISABLED;
    task->menu_items[APPS_MENU_HIDE].flags &=
      (wuss_menu_item_flags_t) ~wuss_MENU_ITEM_DISABLED;
  }
  else
  {
    task->menu_items[APPS_MENU_CLOSE_ALL].flags |= wuss_MENU_ITEM_DISABLED;
    task->menu_items[APPS_MENU_HIDE].flags |= wuss_MENU_ITEM_DISABLED;
  }

  return wuss_iconbar_menu_open(task->delegate, &task->menu,
                                &task->menu_handle);
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
    if (apps_hidden(task)) /* the click only reveals; it opens nothing new */
      return apps_set_hidden(task, 0);
    return apps_open(task, -1, NULL, 0);

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu != &task->menu)
      return result_OK;

    if (event->data.menu_select.index == APPS_MENU_CLOSE_ALL)
    {
      int i;

      for (i = 0; i < (int) NELEMS(task->windows); i++)
        if (task->windows[i].window != NULL)
          (void) wuss_window_try_close(task->windows[i].window);
      return result_OK;
    }
    if (event->data.menu_select.index != APPS_MENU_HIDE)
      return result_OK;

    return apps_set_hidden(task, !apps_hidden(task));

  case wuss_EVENT_MENU_CLOSED:
    task->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->menu_items[APPS_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK || event->data.pre_show.handle == NULL)
      return rc;

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_CLOSE_ADJUST:
  {
    int keep;

    /* as the filer: open the root in this category's place and close this
     * one -- or, with Shift held, cascade the root and keep this one. The
     * root has no parent, so it just closes (or stays with Shift). */
    aw = apps_find(task, window);
    if (aw == NULL)
      return result_OK;

    keep = (event->data.close_adjust.modifiers & wuss_KEY_MOD_SHIFT) != 0;
    if (aw->category >= 0)
      (void) apps_open(task, -1, window, !keep);
    if (!keep)
      return wuss_window_try_close(window);
    return result_OK;
  }

  case wuss_EVENT_CLOSE:
    aw = apps_find(task, window);
    if (aw == NULL)
      return result_OK;

    wuss_gridview_destroy(aw->gridview);
    aw->gridview = NULL;
    aw->window   = NULL;

    /* the root's folder for this category now shows closed */
    wuss_window_invalidate_visible(task->windows[0].window);
    return result_OK;

  case wuss_EVENT_QUIT:
    apps_destroy(task);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

static const bitmap_t *apps_find_glyph(const wuss_task_t *task,
                                       const char        *name)
{
  const bitmap_t *bm;

  bm = wuss_task_iconpool_find(task, name);
  if (bm == NULL)
    logf_warning("appsfiler: glyph '%s' unavailable; cells show label only",
                 name);

  return bm;
}

result_t appsfiler_create(wuss_t *wuss)
{
  result_t                 rc;
  apps_task_t             *task;
  wuss_task_desc_t         desc;
  wuss_iconbar_icon_spec_t spec;
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

  WUSS_MENU_ITEM(task->menu_items, APPS_MENU_CLOSE_ALL, "Close all",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, APPS_MENU_HIDE, "Hide",
                          wuss_MENU_ITEM_NONE, "^H");
  WUSS_MENU_ITEM_WINDOW(task->menu_items, APPS_MENU_INFO, "Info",
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

  task->glyph_directory      = apps_find_glyph(task->delegate, "file_dir");
  task->glyph_directory_open = apps_find_glyph(task->delegate, "file_diro");
  task->glyph_apps           = apps_find_glyph(task->delegate, "apps");

  spec.text  = "Apps";
  spec.side  = wuss_ICONBAR_LEFT;
  spec.image = task->glyph_apps;
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
