/* wuss/tasks/dithering.c -- error-diffusion dithering demo task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/dither.h"
#include "framebuf/octree.h"
#include "io/namelist.h"
#include "io/path.h"
#include "wuss/menu.h"

#include "common.h"
#include "palette.h"

#include "dithering.h"
#include "snapshot.h"
#include "taskapp.h"

/* ----------------------------------------------------------------------- */

/* indexed by dither_method_t */
static const char *const dithering_method_names[dither__LIMIT] =
{
  "None",
  "Floyd-Steinberg",
  "Jarvis-Judice-Ninke",
  "Stucki",
  "Atkinson",
  "Burkes",
  "Sierra"
};

static const int dithering_counts[DITHERING_NCOUNTS] =
{
  2, 4, 8, 16, 64, 256
};
static const char *const dithering_count_names[DITHERING_NCOUNTS] =
{
  "2", "4", "8", "16", "64", "256"
};

static const int dithering_strengths[DITHERING_NSTRENGTHS] =
{
  0, 25, 50, 75, 100
};
static const char *const dithering_strength_names[DITHERING_NSTRENGTHS] =
{
  "0%", "25%", "50%", "75%", "100%"
};

/* ----------------------------------------------------------------------- */

/* Load the PNG at path as rgbx8888/rgba8888. bitmap_load_png keeps a
 * palette-type PNG paletted (p1 to p8), so expand that one. */
static result_t dithering_load(const char *path, bitmap_t *bm)
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

static void dithering_free_dithered(dithering_task_t *dt)
{
  if (dt->dithered == NULL)
    return;

  free(dt->dithered->base);
  free(dt->dithered->palette);
  free(dt->dithered);
  dt->dithered = NULL;
}

/* Build the chosen palette and dither the source with it. On failure the
 * window shows the original instead. */
static result_t dithering_apply(dithering_task_t *dt)
{
  result_t         rc;
  colour_t         palette[256];
  int              ncolours;
  const colour_t  *desktop;
  const char      *name;
  dither_options_t opts;
  bitmap_t        *out;

  if (dt->source.base == NULL)
    return result_OK;

  if (dt->palette == DITHERING_PALETTE_OPTIMAL)
  {
    rc = octree_make_palette(&dt->source, dithering_counts[dt->count],
                             palette, &ncolours);
  }
  else if (dt->palette == DITHERING_PALETTE_DESKTOP)
  {
    desktop  = wuss_get_palette(dt->wuss, &ncolours);
    ncolours = MIN(ncolours, 256);
    memcpy(palette, desktop, ncolours * sizeof(*palette));
    rc = result_OK;
  }
  else
  {
    name = dt->palettes[dt->palette - DITHERING_PALETTE_FIRST_HEX];
    rc = palette_load_hex_n(wuss_get_resources(dt->wuss), name, palette,
                            NELEMS(palette), &ncolours);
  }

  if (rc == result_OK)
  {
    opts.method     = dt->method;
    opts.palette    = palette;
    opts.ncolours   = ncolours;
    opts.strength   = dithering_strengths[dt->strength];
    opts.serpentine = dt->serpentine;
    rc = dither_bitmap(&dt->source, &opts, &out);
  }

  dithering_free_dithered(dt);
  if (rc == result_OK)
    dt->dithered = out;
  else
    logf_warning("dithering: dither failed (rc=0x%X)", rc);

  if (dt->window)
    wuss_window_invalidate_visible(dt->window);

  return result_OK;
}

/* wuss_saveas_save_fn_t: opaque is the dithering_task_t */
static result_t dithering_saveas_save(const char *path, void *opaque)
{
  dithering_task_t *dt;

  dt = opaque;
  if (dt->dithered == NULL)
    return result_BAD_ARG; /* the last dither failed; nothing to save */

  return bitmap_save_png(dt->dithered, path);
}

/* Seed Save As with "<image>-dithered.png". */
static void dithering_saveas_name(dithering_task_t *dt)
{
  char leafname[DITHERING_MAX_NAME_LEN + 16];

  snprintf(leafname, sizeof(leafname), "%s-dithered.png", dt->name);
  wuss_saveas_set_leafname(dt->saveas, leafname);
}

/* Load the PNG at path and dither it, resizing the window to fit. A load
 * failure keeps the current image and is returned. */
static result_t dithering_show(dithering_task_t *dt, const char *path)
{
  result_t rc;
  bitmap_t next;
  char     name[DITHERING_MAX_NAME_LEN];

  if (!path_leaf_strip_ext(path_leaf(path), ".png", name, sizeof(name)))
    return result_BAD_ARG;

  rc = dithering_load(path, &next);
  if (rc != result_OK)
  {
    logf_warning("dithering: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(dt->source.base);
  dt->source = next;
  strcpy(dt->name, name);
  dithering_saveas_name(dt);

  rc = wuss_window_resize(dt->window, dt->source.size);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_set_doc(dt->window, dt->source.size);
  if (rc != result_OK)
    return rc;

  return dithering_apply(dt);
}

/* ----------------------------------------------------------------------- */

result_t dithering_create(wuss_t *wuss, dithering_task_t **out)
{
  result_t          rc;
  dithering_task_t *task;
  wuss_task_desc_t  delegate_desc;
  int               i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss       = wuss;
  task->method     = dither_FLOYD_STEINBERG;
  task->palette    = DITHERING_PALETTE_OPTIMAL;
  task->count      = 3; /* 16 */
  task->strength   = DITHERING_NSTRENGTHS - 1; /* 100% */
  task->serpentine = 1;
  task->source.size = SIZE2D(DITHERING_EMPTYSZ, DITHERING_EMPTYSZ); /* no pixels */

  namelist_scan(pathf("%s/resources/wuss-tasks/palettes",
                      wuss_get_resources(wuss)),
                ".hex", task->palettes[0], sizeof(task->palettes[0]),
                PALETTE_MAX_FILES, 1 /* sorted */, &task->npalettes);

  for (i = 0; i < dither__LIMIT; i++)
    WUSS_MENU_ITEM(task->method_items, i, dithering_method_names[i],
                   wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->method_menu, "Method", task->method_items,
                  dither__LIMIT);

  WUSS_MENU_ITEM(task->palette_items, DITHERING_PALETTE_OPTIMAL, "Optimal",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->palette_items, DITHERING_PALETTE_DESKTOP, "Desktop",
                 wuss_MENU_ITEM_NONE);
  for (i = 0; i < task->npalettes; i++)
    WUSS_MENU_ITEM(task->palette_items, DITHERING_PALETTE_FIRST_HEX + i,
                   task->palettes[i],
                   i == 0 ? wuss_MENU_ITEM_DASHED : wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->palette_menu, "Palette", task->palette_items,
                  DITHERING_PALETTE_FIRST_HEX + task->npalettes);

  for (i = 0; i < DITHERING_NCOUNTS; i++)
    WUSS_MENU_ITEM(task->count_items, i, dithering_count_names[i],
                   wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->count_menu, "Colours", task->count_items,
                  DITHERING_NCOUNTS);

  for (i = 0; i < DITHERING_NSTRENGTHS; i++)
    WUSS_MENU_ITEM(task->strength_items, i, dithering_strength_names[i],
                   wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->strength_menu, "Strength", task->strength_items,
                  DITHERING_NSTRENGTHS);

  WUSS_MENU_ITEM_MENU(task->menu_items, DITHERING_MENU_METHOD, "Method",
                      wuss_MENU_ITEM_NONE, &task->method_menu);
  WUSS_MENU_ITEM_MENU(task->menu_items, DITHERING_MENU_PALETTE, "Palette",
                      wuss_MENU_ITEM_NONE, &task->palette_menu);
  WUSS_MENU_ITEM_MENU(task->menu_items, DITHERING_MENU_COLOURS, "Colours",
                      wuss_MENU_ITEM_NONE, &task->count_menu);
  WUSS_MENU_ITEM_MENU(task->menu_items, DITHERING_MENU_STRENGTH, "Strength",
                      wuss_MENU_ITEM_NONE, &task->strength_menu);
  WUSS_MENU_ITEM(task->menu_items, DITHERING_MENU_SERPENTINE, "Serpentine",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, DITHERING_MENU_SAVE, "Save PNG",
                 wuss_MENU_ITEM_DASHED); /* window set once saveas exists */
  WUSS_MENU_TITLE(task->menu, "Dithering", task->menu_items,
                  DITHERING_MENU__LIMIT);

  delegate_desc.handle    = dithering_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "dithering";
  rc = wuss_task_create(wuss, &delegate_desc, &task->delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  rc = wuss_window_create_at(task->delegate,
                             &task_place,
                             task->source.size,
                             "Dithering",
                             wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                             wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                             task->source.size,
                             SIZE2D(0, 0),
                             &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* QUIT frees the block */
    return rc;
  }

  /* closing the window reaps the task; its QUIT frees the block */
  wuss_task_set_autoclose(task->delegate, 1);

  rc = snapshot_saveas_create(&task->saveas, wuss, NULL, dithering_saveas_save,
                              task);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* QUIT frees the block */
    return rc;
  }
  /* hover opens the Save As dialogue as a submenu */
  task->menu_items[DITHERING_MENU_SAVE].window =
    wuss_saveas_window(task->saveas);

  if (out)
    *out = task;

  return result_OK;
}

result_t dithering_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Dithering",
    "Error-diffusion image dithering",
    (taskapp_create_fn_t) dithering_create,
    (taskapp_data_load_fn_t) dithering_show,
    true
  };

  return taskapp_install(wuss, &desc);
}

void dithering_destroy(dithering_task_t *task)
{
  wuss_saveas_destroy(task->saveas);
  dithering_free_dithered(task);
  free(task->source.base);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t dithering_redraw(dithering_task_t   *dt,
                                 const wuss_event_t *event)
{
  const box_t    *bounds;
  const bitmap_t *bm;

  bounds = event->data.redraw.bounds;
  if (dt->source.base == NULL)
    return result_OK; /* the backdrop shows */

  bm = (dt->show_original || dt->dithered == NULL) ? &dt->source
                                                       : dt->dithered;

  return screen_copy_bitmap(event->data.redraw.scr,
                            bounds->x0 - event->data.redraw.scroll.x,
                            bounds->y0 - event->data.redraw.scroll.y,
                            bm);
}

/* Set the submenus' ticks and the Colours row's state from the task. Run on
 * open and after every pick: a submenu reopened within the same chain (hover
 * away and back) is built afresh from these items, so the live ticks alone
 * would go stale. Colours only applies to the optimal palette, so it is
 * disabled for the others. */
static void dithering_menu_refresh(dithering_task_t *dt)
{
  wuss_menu_item_t *colours;

  colours = &dt->menu_items[DITHERING_MENU_COLOURS];
  if (dt->palette == DITHERING_PALETTE_OPTIMAL)
    colours->flags &= ~wuss_MENU_ITEM_DISABLED;
  else
    colours->flags |= wuss_MENU_ITEM_DISABLED;

  wuss_menu_tick_exclusive(&dt->method_menu, (int) dt->method);
  wuss_menu_tick_exclusive(&dt->palette_menu, dt->palette);
  wuss_menu_tick_exclusive(&dt->count_menu, dt->count);
  wuss_menu_tick_exclusive(&dt->strength_menu, dt->strength);
}

static result_t dithering_menu_open(dithering_task_t *dt)
{
  dithering_menu_refresh(dt);

  return wuss_menu_open_ticked(dt->delegate, &dt->menu,
                               dt->serpentine ? 1u << DITHERING_MENU_SERPENTINE
                                              : 0,
                               wuss_get_pointer(dt->wuss), &dt->menu_handle);
}

static result_t dithering_menu_select(dithering_task_t   *dt,
                                      const wuss_event_t *event)
{
  const wuss_menu_t *menu;
  int                index;

  menu  = event->data.menu_select.menu;
  index = event->data.menu_select.index;

  /* clicking the row, rather than hovering, shows Save As standalone */
  if (menu == &dt->menu && index == DITHERING_MENU_SAVE)
    return wuss_saveas_open(dt->saveas);

  if (menu == &dt->method_menu)
    dt->method = (dither_method_t) index;
  else if (menu == &dt->palette_menu)
    dt->palette = index;
  else if (menu == &dt->count_menu)
    dt->count = index;
  else if (menu == &dt->strength_menu)
    dt->strength = index;
  else if (menu == &dt->menu && index == DITHERING_MENU_SERPENTINE)
    dt->serpentine = !dt->serpentine;
  else
    return result_OK;

  dithering_menu_refresh(dt);
  wuss_menu_disable_item_live(dt->menu_handle, &dt->menu,
                              DITHERING_MENU_COLOURS,
                              dt->palette != DITHERING_PALETTE_OPTIMAL);
  if (menu == &dt->menu)
    wuss_menu_tick_item_live(dt->menu_handle, menu, index, dt->serpentine);
  else
    wuss_menu_tick_exclusive_live(dt->menu_handle, menu, index);

  return dithering_apply(dt);
}

result_t dithering_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  dithering_task_t *dt;

  dt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return dithering_redraw(dt, event);

  case wuss_EVENT_MOUSE:
    if (window != dt->window || event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;

    if (event->data.mouse.button & wuss_BUTTON_SELECT)
    {
      dt->show_original = !dt->show_original;
      wuss_window_invalidate_visible(dt->window);
    }
    else if (event->data.mouse.button & wuss_BUTTON_MENU)
    {
      return dithering_menu_open(dt);
    }
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    return dithering_menu_select(dt, event);

  case wuss_EVENT_MENU_CLOSED:
    dt->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_MESSAGE:
    if (dt->window == NULL || window != dt->window)
      return result_OK;
    return taskapp_window_data_load(dt->wuss, dt->delegate, dt,
                                    (taskapp_data_load_fn_t) dithering_show,
                                    event->data.message);

  case wuss_EVENT_PALETTE:
    if (dt->palette == DITHERING_PALETTE_DESKTOP)
      return dithering_apply(dt);
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == dt->window)
      dt->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    dithering_destroy(dt);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
