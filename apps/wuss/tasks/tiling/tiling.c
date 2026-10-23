/* wuss/tasks/tiling/tiling.c -- tiled backdrop task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "io/path.h"
#include "wuss/menu-desc.h"

#include "tiling.h"
#include "../common/taskapp.h"
#include "../common/common.h"

result_t tiling_create(wuss_t *wuss, tiling_task_t **out)
{
  result_t         rc;
  tiling_task_t   *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  size2d_t         sz;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  delegate_desc.handle    = tiling_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "tiling";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: QUIT is delivered at wuss_destroy instead. */

  sz = wuss_get_screen_size(wuss);

  rc = wuss_window_create_at(delegate, &task_place, sz, "Tiling",
                             wuss_WINDOW_CLOSE | wuss_WINDOW_BACK |
                             wuss_WINDOW_FOCUSABLE,
                             wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW), sz,
                             SIZE2D(32, 32),
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

static result_t tiling_show(tiling_task_t *tc, const char *path);

result_t tiling_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Tiling",
    "Tile screen with PNG",
    (taskapp_create_fn_t) tiling_create,
    (taskapp_data_load_fn_t) tiling_show,
    true
  };

  return taskapp_install(wuss, &desc);
}

void tiling_destroy(tiling_task_t *task)
{
  wuss_menu_destroy(task->menu);
  free(task->bitmap.base);
  free(task); /* task_data was calloc'd per instance by the spawner */
}

/* Load the PNG at path and show it tiled. A PNG that fails to load keeps the
 * current one and the error is returned. */
static result_t tiling_show(tiling_task_t *tc, const char *path)
{
  result_t rc;
  bitmap_t next;
  char     name[64];

  if (!path_leaf_strip_ext(path_leaf(path), ".png", name, sizeof(name)))
    return result_BAD_ARG;

  rc = load_png_deep(&next, path, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    logf_warning("tiling: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(tc->bitmap.base);
  tc->bitmap = next;

  wuss_window_invalidate_visible(tc->window);
  return result_OK;
}

/* Menu shape built from a descriptor string. The tree must outlive the open
 * chain, so it is kept on the task and rebuilt (previous one freed) on each
 * open. */
static result_t tiling_open_menu(tiling_task_t *tc)
{
  result_t     rc;
  wuss_menu_t *m;

  rc = wuss_menu_create_from_desc(&m, "Tiling, Reset origin");
  if (rc != result_OK)
    return rc;

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

  if (tc->bitmap.base == NULL)
    return result_OK; /* the backdrop shows */

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
      if (event->data.mouse.button & wuss_BUTTON_MENU)
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
    if (event->data.menu_select.menu == tc->menu &&
        strcmp(tc->menu->items[event->data.menu_select.index].text,
                    "Reset origin") == 0)
    {
      tc->origin = POINT(0, 0);
      wuss_window_invalidate_visible(tc->window);
    }
    return result_OK;

  case wuss_EVENT_MESSAGE:
    if (tc->window == NULL || window != tc->window)
      return result_OK;
    return taskapp_window_data_load(tc->wuss, tc->delegate, tc,
                                    (taskapp_data_load_fn_t) tiling_show,
                                    event->data.message);

  case wuss_EVENT_MENU_CLOSED:
    tc->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == tc->window)
      tc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    tiling_destroy(tc);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
