/* wuss/tasks/dithering/dithering.c -- error-diffusion dithering demo task */

#ifdef WUSS_APP

#include <math.h>
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
#include "wuss/icon.h"
#include "wuss/menu.h"

#include "../common/common.h"
#include "../palette/palette.h"

#include "dithering.h"
#include "../common/snapshot.h"
#include "../common/taskapp.h"

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

/* ----------------------------------------------------------------------- */

/* Colours slider position to palette size: logarithmic, so each doubling
 * gets the same travel and every power of two is exact */
static int dithering_colours(int pos)
{
  return (int) floor(pow(2.0, (double) pos / DITHERING_COLOURS_STEPS) + 0.5);
}

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
    rc = octree_make_palette(&dt->source, dithering_colours(dt->colours_pos),
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
    opts.strength   = dt->strength;
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

/* ----- slider dialogues ----- */

static result_t dithering_strength_fillout(void *opaque)
{
  dithering_task_t *dt;

  dt = opaque;

  return wuss_slider_row_set(wuss_dialogue_window(dt->strength_dialogue),
                             &dt->strength_row, dt->strength);
}

/* The Colours row echoes its slider position; show the palette size
 * instead. */
static result_t dithering_colours_label(dithering_task_t *dt)
{
  char buf[WUSS_SLIDER_ROW_BUF];

  snprintf(buf, sizeof(buf), "%d", dithering_colours(dt->colours_pos));

  return wuss_icon_set_text(wuss_dialogue_window(dt->colours_dialogue),
                            dt->colours_row.value, buf);
}

static result_t dithering_colours_fillout(void *opaque)
{
  result_t          rc;
  dithering_task_t *dt;

  dt = opaque;

  rc = wuss_slider_row_set(wuss_dialogue_window(dt->colours_dialogue),
                           &dt->colours_row, dt->colours_pos);
  if (rc != result_OK)
    return rc;

  return dithering_colours_label(dt);
}

/* Build a slider dialogue titled and labelled title: the label, a min..max
 * slider and its value echo (fmt, sized to fit widest) in one row. Hover on
 * the menu row opens it as a submenu; every drag re-dithers at once. */
static result_t dithering_slider_create(dithering_task_t           *dt,
                                        const char                 *title,
                                        int                         min,
                                        int                         max,
                                        int                         value,
                                        const char                 *fmt,
                                        const char                 *widest,
                                        wuss_dialogue_fillout_fn_t *fillout,
                                        wuss_dialogue_t           **dialogue,
                                        wuss_slider_row_t          *row)
{
  result_t         rc;
  wuss_icon_spec_t specs[3];
  wuss_icon_t     *made[3];
  char             buf[WUSS_SLIDER_ROW_BUF];
  int              label_w, value_w, width, height;
  box_t            label_box, slider_box, value_box;

  label_w = task_text_width(dt->wuss, title);
  /* +2: a label insets its text a pixel either side, and an overhang would
   * be left behind when the text shrinks */
  value_w = task_text_width(dt->wuss, widest) + 2;
  width   = 2 * wuss_STD_INSET + label_w + DITHERING_SLIDER_W + value_w +
            2 * wuss_STD_GAP;
  height  = 2 * wuss_STD_INSET + wuss_STD_SLIDER_HEIGHT;

  rc = wuss_dialogue_create(dialogue, dt->delegate, SIZE2D(width, height),
                            title, fillout, dt);
  if (rc != result_OK)
    return rc;

  wuss_dialogue_set_focusable(*dialogue, 0);

  label_box  = (box_t) BOX_POS_SIZE(wuss_STD_INSET, wuss_STD_INSET, label_w,
                                    wuss_STD_SLIDER_HEIGHT);
  slider_box = (box_t) BOX_POS_SIZE(label_box.x1 + wuss_STD_GAP,
                                    wuss_STD_INSET, DITHERING_SLIDER_W,
                                    wuss_STD_SLIDER_HEIGHT);
  value_box  = (box_t) BOX_POS_SIZE(slider_box.x1 + wuss_STD_GAP,
                                    wuss_STD_INSET, value_w,
                                    wuss_STD_SLIDER_HEIGHT);

  wuss_icon_spec_label(&specs[0], label_box, title,
                       wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_slider_row(&specs[1], &specs[2], slider_box, value_box,
                            wuss_SLIDER_HORIZONTAL, min, max, value, fmt, 0,
                            buf, sizeof(buf));

  rc = wuss_icon_create_array(wuss_dialogue_window(*dialogue), specs,
                              NELEMS(specs), made);
  if (rc != result_OK)
  {
    wuss_dialogue_destroy(*dialogue);
    *dialogue = NULL;
    return rc;
  }

  wuss_slider_row_bind(row, made[1], made[2], fmt, min, max, 0);

  return result_OK;
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
  task->colours_pos = 4 * DITHERING_COLOURS_STEPS; /* 16 */
  task->strength   = 100;
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

  WUSS_MENU_ITEM_MENU(task->menu_items, DITHERING_MENU_METHOD, "Method",
                      wuss_MENU_ITEM_NONE, &task->method_menu);
  WUSS_MENU_ITEM_MENU(task->menu_items, DITHERING_MENU_PALETTE, "Palette",
                      wuss_MENU_ITEM_NONE, &task->palette_menu);
  /* windows set once the dialogues exist */
  WUSS_MENU_ITEM(task->menu_items, DITHERING_MENU_COLOURS, "Colours",
                 wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);
  WUSS_MENU_ITEM(task->menu_items, DITHERING_MENU_STRENGTH, "Strength",
                 wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);
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

  rc = dithering_slider_create(task, "Strength", 0, 100, task->strength,
                               "%d%%", "100%", dithering_strength_fillout,
                               &task->strength_dialogue, &task->strength_row);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* QUIT frees the block */
    return rc;
  }
  task->menu_items[DITHERING_MENU_STRENGTH].window =
    wuss_dialogue_window(task->strength_dialogue);

  /* 2..256 colours; the fillout replaces the position echo with the size */
  rc = dithering_slider_create(task, "Colours", 1 * DITHERING_COLOURS_STEPS,
                               8 * DITHERING_COLOURS_STEPS, task->colours_pos,
                               "%d", "256", dithering_colours_fillout,
                               &task->colours_dialogue, &task->colours_row);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* QUIT frees the block */
    return rc;
  }
  task->menu_items[DITHERING_MENU_COLOURS].window =
    wuss_dialogue_window(task->colours_dialogue);

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
  wuss_dialogue_destroy(task->colours_dialogue);
  wuss_dialogue_destroy(task->strength_dialogue);
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
  result_t          rc;
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

  case wuss_EVENT_ICON:
    if (window == wuss_dialogue_window(dt->strength_dialogue) &&
        wuss_slider_row_event(window, &dt->strength_row, event,
                              &dt->strength))
      return dithering_apply(dt);

    if (window == wuss_dialogue_window(dt->colours_dialogue) &&
        wuss_slider_row_event(window, &dt->colours_row, event,
                              &dt->colours_pos))
    {
      rc = dithering_colours_label(dt);
      if (rc != result_OK)
        return rc;

      return dithering_apply(dt);
    }
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (window == wuss_dialogue_window(dt->strength_dialogue))
      rc = wuss_dialogue_handle_pre_show(dt->strength_dialogue);
    else if (window == wuss_dialogue_window(dt->colours_dialogue))
      rc = wuss_dialogue_handle_pre_show(dt->colours_dialogue);
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;

    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);

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
    wuss_dialogue_handle_close(dt->strength_dialogue, window);
    wuss_dialogue_handle_close(dt->colours_dialogue, window);
    if (window == dt->window)
    {
      dt->window = NULL;
      /* the hidden dialogues are more windows on this autoclose task:
       * close them too, else the task outlives its main window */
      wuss_dialogue_destroy(dt->strength_dialogue);
      dt->strength_dialogue = NULL;
      wuss_dialogue_destroy(dt->colours_dialogue);
      dt->colours_dialogue = NULL;
    }
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
