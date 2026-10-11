/* wuss/test/tasks/effects.c -- bitmap effects task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/effects.h"
#include "framebuf/tonemap.h"
#include "io/filetype.h"
#include "io/path.h"
#include "wuss/menu.h"
#include "wuss/menu-desc.h"

#include "effects.h"
#include "taskapp.h"
#include "common.h"

static const int g_amounts[EFFECTS_NAMOUNTS] = { 2, 4, 8, 16, 32, 48 };

/* ----------------------------------------------------------------------- */

/* Run the chosen effect from source into result. */
static result_t effects_apply(effects_task_t *ec)
{
  result_t       rc;
  tonemap_t     *map;
  tonemap_spec_t spec;
  int            amount;

  if (ec->source.base == NULL)
    return result_OK;

  amount = g_amounts[ec->amount];

  switch (ec->effect)
  {
  case EFFECTS_BLUR_BOX:
    return effects_blur_apply(&ec->source, &ec->result, effects_blur_BOX,
                              amount);

  case EFFECTS_BLUR_GAUSSIAN:
    return effects_blur_apply(&ec->source, &ec->result,
                              effects_blur_GAUSSIAN, amount);

  case EFFECTS_SHARPEN:
    return effects_sharpen_apply(&ec->source, &ec->result);

  case EFFECTS_GREY:
    return effects_grey_apply(&ec->source, &ec->result);

  case EFFECTS_EXPAND:
    return effects_expand_apply(&ec->source, &ec->result, 0);

  case EFFECTS_EQUALISE:
    return effects_equalise_apply(&ec->source, &ec->result);

  case EFFECTS_BRIGHTEN:
  case EFFECTS_INVERT:
    map = tonemap_create();
    if (map == NULL)
      return result_OOM;

    tonemap_get_values(map, tonemap_CHANNEL_RGB, &spec);
    if (ec->effect == EFFECTS_BRIGHTEN)
      spec.gamma = 200;
    else
      spec.flags = tonemap_FLAG_INVERT;
    tonemap_set(map, tonemap_CHANNEL_RGB, &spec);

    rc = effects_tonemap_apply(&ec->source, &ec->result, map);

    tonemap_destroy(map);

    return rc;

  default:
    memcpy(ec->result.base, ec->source.base,
           (size_t) ec->source.rowbytes * ec->source.size.h);
    return result_OK;
  }
}

/* Allocate result to match source, then apply. */
static result_t effects_rebuild(effects_task_t *ec)
{
  free(ec->result.base);

  ec->result      = ec->source;
  ec->result.base = malloc((size_t) ec->source.rowbytes * ec->source.size.h);
  if (ec->result.base == NULL)
    return result_OOM;

  return effects_apply(ec);
}

/* wuss_saveas_save_fn_t: opaque is the effects_task_t */
static result_t effects_saveas_save(const char *path, void *opaque)
{
  effects_task_t *ec;

  ec = opaque;
  if (ec->result.base == NULL)
    return result_BAD_ARG; /* nothing loaded yet; nothing to save */

  return bitmap_save_png(&ec->result, path);
}

/* Seed Save As with "<image>-effects.png". */
static void effects_saveas_name(effects_task_t *ec)
{
  char leafname[EFFECTS_MAX_NAME_LEN + 16];

  snprintf(leafname, sizeof(leafname), "%s-effects.png", ec->name);
  wuss_saveas_set_leafname(ec->saveas, leafname);
}

/* Load the PNG at path and re-apply, resizing the window to fit. A load
 * failure keeps the current image and is returned. */
static result_t effects_show(effects_task_t *ec, const char *path)
{
  result_t rc;
  bitmap_t next;
  char     name[EFFECTS_MAX_NAME_LEN];

  if (!path_leaf_strip_ext(path_leaf(path), ".png", name, sizeof(name)))
    return result_BAD_ARG;

  rc = load_png_deep(&next, path, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    logf_warning("effects: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(ec->source.base);
  ec->source = next;
  strcpy(ec->name, name);
  effects_saveas_name(ec);

  rc = effects_rebuild(ec);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_resize(ec->window, ec->source.size);
  if (rc != result_OK)
    return rc;

  wuss_window_invalidate_visible(ec->window);
  return wuss_window_set_doc(ec->window, ec->source.size);
}

/* ----------------------------------------------------------------------- */

result_t effects_create(wuss_t *wuss, effects_task_t **out)
{
  result_t         rc;
  effects_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  filetype_t       png_type;
  size2d_t         sz;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss   = wuss;
  task->effect = EFFECTS_BLUR_GAUSSIAN;
  task->amount = 2;

  WUSS_MENU_ITEM(task->effect_items, EFFECTS_NONE, "None",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_BLUR_BOX, "Box blur",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_BLUR_GAUSSIAN, "Gaussian blur",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_SHARPEN, "Sharpen",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_GREY, "Grey",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_EXPAND, "Expand",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_EQUALISE, "Equalise",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_BRIGHTEN, "Brighten",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->effect_items, EFFECTS_INVERT, "Invert",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->effect_menu, "Effect", task->effect_items,
                  NELEMS(task->effect_items));

  for (i = 0; i < EFFECTS_NAMOUNTS; i++)
  {
    snprintf(task->amount_text[i], sizeof(task->amount_text[i]), "%d", g_amounts[i]);
    WUSS_MENU_ITEM(task->amount_items, i, task->amount_text[i],
                   wuss_MENU_ITEM_NONE);
  }
  WUSS_MENU_TITLE(task->amount_menu, "Amount", task->amount_items,
                  NELEMS(task->amount_items));

  delegate_desc.handle    = effects_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "effects";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: QUIT is delivered at wuss_destroy instead. */

  sz = SIZE2D(EFFECTS_EMPTYSZ, EFFECTS_EMPTYSZ);

  rc = wuss_window_create_placed(delegate,
                                 sz,
                                 "Effects",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 sz,
                                 SIZE2D(32, 32),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the bitmaps and the block */
    return rc;
  }

  png_type = filetype_from_ext(".png");
  rc = wuss_saveas_create(&task->saveas, wuss, &png_type, NULL,
                          effects_saveas_save, task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

result_t effects_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Effects",
    "Apply effects to PNGs",
    (taskapp_create_fn_t) effects_create,
    (taskapp_data_load_fn_t) effects_show,
    true
  };

  return taskapp_install(wuss, &desc);
}

void effects_destroy(effects_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  wuss_menu_destroy(task->menu);
  free(task->source.base);
  free(task->result.base);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t effects_redraw(const wuss_event_t *event, void *task_data)
{
  effects_task_t *ec;
  screen_t       *scr;
  const box_t    *bounds;
  const bitmap_t *bm;

  ec = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  if (ec->source.base == NULL)
    return result_OK; /* the backdrop shows */

  bm = ec->show_original ? &ec->source : &ec->result;

  return screen_copy_bitmap_dithered(scr,
                                     bounds->x0 - event->data.redraw.scroll.x,
                                     bounds->y0 - event->data.redraw.scroll.y,
                                     bm,
                                     screen_DITHER_BLUE_NOISE);
}

/* Menu shape built from a descriptor string; the submenus are borrowed. */
static result_t effects_open_menu(effects_task_t *ec)
{

  result_t     rc;
  wuss_menu_t *m;
  int          i;

  rc = wuss_menu_create_from_desc(&m, "Effects, Effect, Amount, |Save PNG");
  if (rc != result_OK)
    return rc;

  wuss_menu_tick_exclusive(&ec->effect_menu, ec->effect);
  wuss_menu_tick_exclusive(&ec->amount_menu, ec->amount);

  for (i = 0; i < m->nitems; i++)
  {
    wuss_menu_item_t *it = (wuss_menu_item_t *) &m->items[i];

    if (it->text == NULL)
      continue;
    if (strcmp(it->text, "Save PNG") == 0)
    {
      it->window = wuss_saveas_window(ec->saveas);
      continue;
    }
    if (strcmp(it->text, "Effect") == 0)
      it->submenu = &ec->effect_menu;
    else if (strcmp(it->text, "Amount") == 0)
      it->submenu = &ec->amount_menu;
    else
      continue;
    it->flags |= wuss_MENU_ITEM_BORROWED_SUBMENU;
  }

  wuss_menu_destroy(ec->menu);
  ec->menu = m;

  return wuss_menu_open_at_pointer(ec->delegate, ec->menu, &ec->menu_handle);
}

result_t effects_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  result_t           rc;
  effects_task_t    *ec;
  const wuss_menu_t *menu;
  int                index;

  ec = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return effects_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != ec->window)
      return result_OK;
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
    {
      ec->show_original = !ec->show_original;
      wuss_window_invalidate_visible(ec->window);
      return result_OK;
    }
    if (event->data.mouse.button & wuss_BUTTON_MENU)
      return effects_open_menu(ec);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    menu  = event->data.menu_select.menu;
    index = event->data.menu_select.index;
    if (menu == ec->menu && ec->menu->items[index].window ==
        wuss_saveas_window(ec->saveas))
      return wuss_saveas_open(ec->saveas); /* a click, not a hover */
    if (menu == &ec->effect_menu)
      ec->effect = index;
    else if (menu == &ec->amount_menu)
      ec->amount = index;
    else
      return result_OK;

    ec->show_original = 0;
    wuss_menu_tick_exclusive(&ec->effect_menu, ec->effect);
    wuss_menu_tick_exclusive(&ec->amount_menu, ec->amount);
    wuss_menu_tick_exclusive_live(ec->menu_handle, &ec->effect_menu,
                                  ec->effect);
    wuss_menu_tick_exclusive_live(ec->menu_handle, &ec->amount_menu,
                                  ec->amount);

    rc = effects_apply(ec);
    wuss_window_invalidate_visible(ec->window);
    return rc;

  case wuss_EVENT_MESSAGE:
    if (ec->window == NULL || window != ec->window)
      return result_OK;
    return taskapp_window_data_load(ec->wuss, ec->delegate, ec,
                                    (taskapp_data_load_fn_t) effects_show,
                                    event->data.message);

  case wuss_EVENT_MENU_CLOSED:
    ec->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == ec->window)
      ec->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (ec->menu == NULL)
      return result_OK;
    for (index = 0; index < ec->menu->nitems; index++)
    {
      if (ec->menu->items[index].window != window)
        continue;
      if (window != wuss_saveas_window(ec->saveas))
        return result_OK;
      if (event->data.pre_show.handle == NULL)
        return result_OK;
      return wuss_menu_open_window_now(event->data.pre_show.handle,
                                       event->data.pre_show.index);
    }
    return result_OK;

  case wuss_EVENT_QUIT:
    effects_destroy(ec);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
