/* wuss/test/tasks/tiling.c -- tiled backdrop task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "io/namelist.h"
#include "io/path.h"
#include "wuss/menu-desc.h"

#include "tiling.h"
#include "common.h"

#define TILING_EXT ".png"

result_t tiling_create(wuss_t *wuss, tiling_task_t **out)
{
  result_t         rc;
  tiling_task_t   *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  const char      *resources;
  size2d_t         sz;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  resources = wuss_get_resources(wuss);
  rc = namelist_scan(pathf("%s/resources/images", resources), TILING_EXT,
                     task->names[0], sizeof(task->names[0]),
                     TILING_MAX_NAMES, 0 /* unsorted */, &task->nnames);
  if (rc == result_OK && task->nnames == 0)
    rc = result_BAD_ARG; /* no PNGs found under resources/images */
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  for (i = 0; i < task->nnames; i++)
    WUSS_MENU_ITEM(task->load_items, i, task->names[i], wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->load_menu, "Load", task->load_items, task->nnames);

  rc = load_png_deep(&task->bitmap,
                     pathf("%s/resources/images/%s" TILING_EXT, resources,
                           task->names[0]),
                     pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }

  delegate_desc.handle    = tiling_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "tiling";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task);
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: QUIT is delivered at wuss_destroy instead. */

  sz = wuss_get_screen_size(wuss);

  rc = wuss_window_create_placed(delegate, sz, "Tiling",
                                 wuss_WINDOW_CLOSE | wuss_WINDOW_BACK |
                                 wuss_WINDOW_FOCUSABLE,
                                 wuss_NO_BACKDROP, sz, SIZE2D(32, 32),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the bitmap and the block */
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

void tiling_destroy(tiling_task_t *task)
{
  wuss_menu_destroy(task->menu);
  free(task->bitmap.base);
  free(task); /* task_data was calloc'd per instance by the spawner */
}

/* Load and show names[index]. A PNG that fails to load keeps the current one. */
static void tiling_show(tiling_task_t *tc, int index)
{
  result_t    rc;
  bitmap_t    next;
  const char *filename;

  filename = pathf("%s/resources/images/%s" TILING_EXT,
                   wuss_get_resources(tc->wuss), tc->names[index]);

  rc = load_png_deep(&next, filename, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    logf_warning("tiling: skipping \"%s\" (rc=0x%X)", filename, rc);
    return;
  }

  free(tc->bitmap.base);
  tc->bitmap = next;
  tc->index  = index;

  wuss_window_invalidate_visible(tc->window);
}

/* Menu shape built from a descriptor string. The tree must outlive the open
 * chain, so it is kept on the task and rebuilt (previous one freed) on each
 * open. */
static result_t tiling_open_menu(tiling_task_t *tc)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Tiling", "Fill the screen with a repeated PNG");

  result_t     rc;
  wuss_menu_t *m;
  int          i;

  rc = wuss_menu_create_from_desc(&m, "Tiling, Info, Load, Reset origin");
  if (rc != result_OK)
    return rc;

  wuss_proginfo_set_desc(&desc);
  wuss_menu_tick_exclusive(&tc->load_menu, tc->index);
  for (i = 0; i < m->nitems; i++)
  {
    wuss_menu_item_t *it = (wuss_menu_item_t *) &m->items[i];

    if (it->text == NULL)
      continue;
    if (strcmp(it->text, "Info") == 0)
    {
      it->window = wuss_proginfo_window(tc->delegate);
    }
    else if (strcmp(it->text, "Load") == 0)
    {
      it->submenu = &tc->load_menu;
      it->flags  |= wuss_MENU_ITEM_BORROWED_SUBMENU;
    }
  }

  wuss_menu_destroy(tc->menu);
  tc->menu = m;

  return wuss_menu_open_at_pointer(tc->delegate, tc->menu, &tc->menu_handle);
}

static result_t tiling_redraw(const wuss_event_t *event, void *task_data)
{
  tiling_task_t *tc;
  const box_t   *bounds;
  point_t        origin;

  tc     = task_data;
  bounds = event->data.redraw.bounds;

  origin = POINT(bounds->x0 - event->data.redraw.scroll.x + tc->origin.x,
                 bounds->y0 - event->data.redraw.scroll.y + tc->origin.y);

  return screen_copy_bitmap_tiled(event->data.redraw.scr, bounds, &tc->bitmap,
                                  origin);
}

result_t tiling_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  tiling_task_t *tc;
  int            i;

  tc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return tiling_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != tc->window)
      return result_OK;

    if (event->data.mouse.action == wuss_MOUSE_UP)
    {
      tc->dragging = 0;
    }
    else if (event->data.mouse.action == wuss_MOUSE_DOWN)
    {
      if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      {
        tiling_show(tc, (tc->index + 1) % tc->nnames);
      }
      else if (event->data.mouse.button & wuss_BUTTON_MENU)
      {
        return tiling_open_menu(tc);
      }
      else if (event->data.mouse.button & wuss_BUTTON_SELECT)
      {
        tc->dragging = 1;
        tc->grab     = event->data.mouse.point;
      }
    }
    else if (tc->dragging) /* wuss_MOUSE_MOVE */
    {
      tc->origin.x += event->data.mouse.point.x - tc->grab.x;
      tc->origin.y += event->data.mouse.point.y - tc->grab.y;
      tc->grab      = event->data.mouse.point;
      wuss_window_invalidate_visible(tc->window);
    }
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &tc->load_menu)
    {
      i = event->data.menu_select.index;
      tiling_show(tc, i);
      wuss_menu_tick_exclusive(&tc->load_menu, tc->index);
      wuss_menu_tick_exclusive_live(tc->menu_handle, &tc->load_menu,
                                    tc->index);
    }
    else if (event->data.menu_select.menu == tc->menu &&
             strcmp(tc->menu->items[event->data.menu_select.index].text,
                    "Reset origin") == 0)
    {
      tc->origin = POINT(0, 0);
      wuss_window_invalidate_visible(tc->window);
    }
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    tc->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (tc->menu != NULL)
      for (i = 0; i < tc->menu->nitems; i++)
        if (tc->menu->items[i].window == window)
          return wuss_proginfo_handle_pre_show();
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == tc->window)
      tc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    tiling_destroy(tc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
