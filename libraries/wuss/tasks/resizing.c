/* wuss/tasks/resizing.c -- image resizing demo task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "io/path.h"
#include "wuss/menu.h"

#include "common.h"

#include "resizing.h"
#include "snapshot.h"
#include "taskapp.h"

/* ----------------------------------------------------------------------- */

/* indexed by bitmap_resample_t */
static const char *const resizing_method_names[bitmap_resample__LIMIT] =
{
  "Nearest",
  "Bilinear",
  "Box",
  "Catmull-Rom"
};

static const int resizing_scales[RESIZING_NSCALES] =
{
  25, 50, 75, 100, 150, 200, 300, 400
};
static const char *const resizing_scale_names[RESIZING_NSCALES] =
{
  "25%", "50%", "75%", "100%", "150%", "200%", "300%", "400%"
};

/* ----------------------------------------------------------------------- */

/* Load the PNG at path as rgbx8888/rgba8888. bitmap_load_png keeps a
 * palette-type PNG paletted (p1 to p8), so expand that one. */
static result_t resizing_load(const char *path, bitmap_t *bm)
{
  result_t  rc;
  bitmap_t *deep;

  rc = bitmap_load_png(bm, path);
  if (rc != result_OK)
    return rc;

  if (bm->format < pixelfmt_p1 || bm->format > pixelfmt_p8)
    return result_OK;

  rc = bitmap_convert(bm, pixelfmt_rgbx8888, &deep);
  free(bm->base);
  free(bm->palette);
  if (rc != result_OK)
    return rc;

  *bm = *deep;
  free(deep);

  return result_OK;
}

static void resizing_free_resized(resizing_task_t *rt)
{
  if (rt->resized == NULL)
    return;

  free(rt->resized->base);
  free(rt->resized->palette);
  free(rt->resized);
  rt->resized = NULL;
}

/* Resample the source to the chosen scale and fit the window to it. On
 * failure the window shows the original instead. */
static result_t resizing_apply(resizing_task_t *rt)
{
  result_t  rc;
  size2d_t  size;
  bitmap_t *out;
  int       percent;

  if (rt->source.base == NULL)
    return result_OK;

  percent = resizing_scales[rt->scale];
  size.w  = MAX(1, rt->source.size.w * percent / 100);
  size.h  = MAX(1, rt->source.size.h * percent / 100);

  rc = bitmap_resize(&rt->source, size, rt->method, rt->linear, &out);

  resizing_free_resized(rt);
  if (rc == result_OK)
    rt->resized = out;
  else
    logf_warning("resizing: resize failed (rc=0x%X)", rc);

  if (rt->window == NULL)
    return result_OK;

  size = rt->resized ? rt->resized->size : rt->source.size;
  rc   = wuss_window_resize(rt->window, size);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_set_doc(rt->window, size);
  if (rc != result_OK)
    return rc;

  wuss_window_invalidate_visible(rt->window);

  return result_OK;
}

/* wuss_saveas_save_fn_t: opaque is the resizing_task_t */
static result_t resizing_saveas_save(const char *path, void *opaque)
{
  resizing_task_t *rt;

  rt = opaque;
  if (rt->resized == NULL)
    return result_BAD_ARG; /* the last resize failed; nothing to save */

  return bitmap_save_png(rt->resized, path);
}

/* Seed Save As with "<image>-resized.png". */
static void resizing_saveas_name(resizing_task_t *rt)
{
  char leafname[RESIZING_MAX_NAME_LEN + 16];

  snprintf(leafname, sizeof(leafname), "%s-resized.png", rt->name);
  wuss_saveas_set_leafname(rt->saveas, leafname);
}

/* Load the PNG at path and resize it. A load failure keeps the current
 * image and is returned. */
static result_t resizing_show(resizing_task_t *rt, const char *path)
{
  result_t rc;
  bitmap_t next;
  char     name[RESIZING_MAX_NAME_LEN];

  if (!path_leaf_strip_ext(path_leaf(path), ".png", name, sizeof(name)))
    return result_BAD_ARG;

  rc = resizing_load(path, &next);
  if (rc != result_OK)
  {
    logf_warning("resizing: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(rt->source.base);
  rt->source = next;
  strcpy(rt->name, name);
  resizing_saveas_name(rt);

  return resizing_apply(rt);
}

/* ----------------------------------------------------------------------- */

result_t resizing_create(wuss_t *wuss, resizing_task_t **out)
{
  result_t         rc;
  resizing_task_t *task;
  wuss_task_desc_t delegate_desc;
  size2d_t         empty;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  empty              = SIZE2D(RESIZING_EMPTYSZ, RESIZING_EMPTYSZ);
  task->wuss         = wuss;
  task->method       = bitmap_resample_BILINEAR;
  task->scale        = 3; /* 100% */
  task->linear       = 1;
  task->source.size  = empty; /* no pixels */

  for (i = 0; i < bitmap_resample__LIMIT; i++)
    WUSS_MENU_ITEM(task->method_items, i, resizing_method_names[i],
                   wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->method_menu, "Method", task->method_items,
                  bitmap_resample__LIMIT);

  for (i = 0; i < RESIZING_NSCALES; i++)
    WUSS_MENU_ITEM(task->scale_items, i, resizing_scale_names[i],
                   wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->scale_menu, "Scale", task->scale_items,
                  RESIZING_NSCALES);

  WUSS_MENU_ITEM_MENU(task->menu_items, RESIZING_MENU_METHOD, "Method",
                      wuss_MENU_ITEM_NONE, &task->method_menu);
  WUSS_MENU_ITEM_MENU(task->menu_items, RESIZING_MENU_SCALE, "Scale",
                      wuss_MENU_ITEM_NONE, &task->scale_menu);
  WUSS_MENU_ITEM(task->menu_items, RESIZING_MENU_LINEAR, "Linear light",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, RESIZING_MENU_SAVE, "Save PNG",
                 wuss_MENU_ITEM_DASHED); /* window set once saveas exists */
  WUSS_MENU_TITLE(task->menu, "Resizing", task->menu_items,
                  RESIZING_MENU__LIMIT);

  delegate_desc.handle    = resizing_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "resizing";
  rc = wuss_task_create(wuss, &delegate_desc, &task->delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  rc = wuss_window_create_at(task->delegate,
                             &task_place,
                             empty,
                             "Resizing",
                             wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                             wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                             empty,
                             SIZE2D(0, 0),
                             &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* QUIT frees the block */
    return rc;
  }

  /* closing the window reaps the task; its QUIT frees the block */
  wuss_task_set_autoclose(task->delegate, 1);

  rc = snapshot_saveas_create(&task->saveas, wuss, NULL, resizing_saveas_save,
                              task);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* QUIT frees the block */
    return rc;
  }
  /* hover opens the Save As dialogue as a submenu */
  task->menu_items[RESIZING_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  if (out)
    *out = task;

  return result_OK;
}

result_t resizing_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Resizing",
    "Image resizing",
    (taskapp_create_fn_t) resizing_create,
    (taskapp_data_load_fn_t) resizing_show,
    true
  };

  return taskapp_install(wuss, &desc);
}

void resizing_destroy(resizing_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  resizing_free_resized(task);
  free(task->source.base);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t resizing_redraw(resizing_task_t    *rt,
                                const wuss_event_t *event)
{
  const box_t    *bounds;
  const bitmap_t *bm;

  bounds = event->data.redraw.bounds;
  if (rt->source.base == NULL)
    return result_OK; /* the backdrop shows */

  bm = rt->resized != NULL ? rt->resized : &rt->source;

  return screen_copy_bitmap(event->data.redraw.scr,
                            bounds->x0 - event->data.redraw.scroll.x,
                            bounds->y0 - event->data.redraw.scroll.y,
                            bm);
}

/* Set the submenus' ticks from the task. Run on open and after every pick: a
 * submenu reopened within the same chain (hover away and back) is built
 * afresh from these items, so the live ticks alone would go stale. */
static void resizing_menu_refresh(resizing_task_t *rt)
{
  wuss_menu_tick_exclusive(&rt->method_menu, (int) rt->method);
  wuss_menu_tick_exclusive(&rt->scale_menu, rt->scale);
}

static result_t resizing_menu_open(resizing_task_t *rt)
{
  resizing_menu_refresh(rt);

  return wuss_menu_open_ticked(rt->delegate, &rt->menu,
                               rt->linear ? 1u << RESIZING_MENU_LINEAR : 0,
                               wuss_get_pointer(rt->wuss), &rt->menu_handle);
}

static result_t resizing_menu_select(resizing_task_t    *rt,
                                     const wuss_event_t *event)
{
  const wuss_menu_t *menu;
  int                index;

  menu  = event->data.menu_select.menu;
  index = event->data.menu_select.index;

  /* clicking the row, rather than hovering, shows Save As standalone */
  if (menu == &rt->menu && index == RESIZING_MENU_SAVE)
    return wuss_saveas_open(rt->saveas);

  if (menu == &rt->method_menu)
    rt->method = (bitmap_resample_t) index;
  else if (menu == &rt->scale_menu)
    rt->scale = index;
  else if (menu == &rt->menu && index == RESIZING_MENU_LINEAR)
    rt->linear = !rt->linear;
  else
    return result_OK;

  resizing_menu_refresh(rt);
  if (menu == &rt->menu)
    wuss_menu_tick_item_live(rt->menu_handle, menu, index, rt->linear);
  else
    wuss_menu_tick_exclusive_live(rt->menu_handle, menu, index);

  return resizing_apply(rt);
}

result_t resizing_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  resizing_task_t *rt;

  rt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return resizing_redraw(rt, event);

  case wuss_EVENT_MOUSE:
    if (window != rt->window || event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;

    if (event->data.mouse.button & wuss_BUTTON_MENU)
      return resizing_menu_open(rt);

    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    return resizing_menu_select(rt, event);

  case wuss_EVENT_MENU_CLOSED:
    rt->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_MESSAGE:
    if (rt->window == NULL || window != rt->window)
      return result_OK;
    return taskapp_window_data_load(rt->wuss, rt->delegate, rt,
                                    (taskapp_data_load_fn_t) resizing_show,
                                    event->data.message);

  case wuss_EVENT_CLOSE:
    if (window == rt->window)
      rt->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    resizing_destroy(rt);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
