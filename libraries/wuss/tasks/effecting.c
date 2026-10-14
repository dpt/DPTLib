/* wuss/tasks/effecting.c -- bitmap effects task */

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
#include "io/path.h"
#include "utils/array.h"
#include "wuss/component/colourmenu.h"
#include "wuss/gadget/gridview.h"
#include "wuss/menu.h"

#include "effecting.h"
#include "effecting-edit.h"
#include "snapshot.h"
#include "taskapp.h"
#include "common.h"

#define EFFECTING_LABEL_CHARS 22
#define EFFECTING_LIST_MIN_W  150 /* content width that fits the title */
#define EFFECTING_LIST_ROWS   8

static const char *const g_names[EFFECTING__LIMIT] =
{
  "Clear",
  "Tone",
  "Grey",
  "Blur",
  "Sharpen",
  "Expand",
  "Equalise"
};

/* ----------------------------------------------------------------------- */

/* Run one element from src into dst, which may be the same bitmap. */
static result_t effecting_apply_element(const effecting_element_t *el,
                                        const bitmap_t            *src,
                                        bitmap_t                  *dst)
{
  switch (el->type)
  {
  case EFFECTING_CLEAR:
    return effects_clear_apply(src, dst, el->u.clear);

  case EFFECTING_TONE:
    return effects_tonemap_apply(src, dst, el->u.tone);

  case EFFECTING_GREY:
    return effects_grey_apply(src, dst);

  case EFFECTING_BLUR:
    return effects_blur_apply(src, dst, el->u.blur.method, el->u.blur.amount);

  case EFFECTING_SHARPEN:
    return effects_sharpen_apply(src, dst);

  case EFFECTING_EXPAND:
    return effects_expand_apply(src, dst, 0);

  case EFFECTING_EQUALISE:
    return effects_equalise_apply(src, dst);

  default:
    return result_BAD_ARG;
  }
}

/* Run the stack from source into chain (if rechain), then blend source and
 * chain into result by the level. */
static result_t effecting_run(effecting_task_t *ec, int rechain)
{
  result_t rc;
  int      i;

  if (ec->source.base == NULL)
    return result_OK;

  if (rechain)
  {
    if (ec->nelements == 0)
      memcpy(ec->chain.base, ec->source.base,
             (size_t) ec->source.rowbytes * ec->source.size.h);

    for (i = 0; i < ec->nelements; i++)
    {
      rc = effecting_apply_element(&ec->elements[i],
                                   i == 0 ? &ec->source : &ec->chain,
                                   &ec->chain);
      if (rc != result_OK)
        return rc;
    }
  }

  return effects_blend_extrapolate(&ec->source, &ec->chain, &ec->result,
                                   ec->level * 256 / 100);
}

void effecting_update(effecting_task_t *ec, int rechain)
{
  result_t rc;

  rc = effecting_run(ec, rechain);
  if (rc != result_OK)
    logf_warning("effecting: cannot apply effects (rc=0x%X)", rc);

  ec->show_original = 0;
  if (ec->window != NULL)
    wuss_window_invalidate_visible(ec->window);
}

/* Allocate chain and result to match source, then run. */
static result_t effecting_rebuild(effecting_task_t *ec)
{
  size_t size;

  free(ec->chain.base);
  free(ec->result.base);

  size = (size_t) ec->source.rowbytes * ec->source.size.h;

  ec->chain       = ec->source;
  ec->chain.base  = malloc(size);
  ec->result      = ec->source;
  ec->result.base = malloc(size);
  if (ec->chain.base == NULL || ec->result.base == NULL)
  {
    free(ec->chain.base);
    free(ec->result.base);
    ec->chain.base  = NULL;
    ec->result.base = NULL;

    return result_OOM;
  }

  return effecting_run(ec, 1);
}

/* ----- the stack ----- */

effecting_element_t *effecting_selected(effecting_task_t *ec, int *index)
{
  int i;

  if (ec->list == NULL)
    return NULL;

  for (i = 0; i < ec->nelements; i++)
  {
    if (!wuss_gridview_is_selected(ec->list, i))
      continue;

    if (index != NULL)
      *index = i;

    return &ec->elements[i];
  }

  return NULL;
}

/* wuss_gridview_item_fn_t: opaque is the effecting_task_t */
static void effecting_item(int                   index,
                           void                 *opaque,
                           wuss_gridview_item_t *out)
{
  effecting_task_t          *ec;
  const effecting_element_t *el;
  const char                *name;

  ec   = opaque;
  el   = &ec->elements[index];
  name = g_names[el->type];

  switch (el->type)
  {
  case EFFECTING_BLUR:
    snprintf(ec->label, sizeof(ec->label), "%s blur %d",
             el->u.blur.method == effects_blur_BOX ? "Box" : "Gaussian",
             el->u.blur.amount);
    break;

  case EFFECTING_CLEAR:
  {
    unsigned int r, g, b;

    colour_get_rgb(&el->u.clear, &r, &g, &b);
    snprintf(ec->label, sizeof(ec->label), "%s %02X%02X%02X", name, r, g, b);
    break;
  }

  default:
    snprintf(ec->label, sizeof(ec->label), "%s", name);
    break;
  }

  out->glyph = NULL;
  out->label = ec->label;
}

static void effecting_menu_update(effecting_task_t *ec);

static void effecting_list_changed(effecting_task_t *ec)
{
  if (ec->list != NULL)
    (void) wuss_gridview_set_count(ec->list, ec->nelements);

  /* an open menu must reflect the new stack and selection */
  effecting_menu_update(ec);
}

/* Sizes the list window to its rows, but never too narrow for its title. */
static void effecting_fit_list(effecting_task_t *ec)
{
  box_t    bounds;
  size2d_t sz;

  wuss_gridview_fit_window(ec->list, 1, EFFECTING_LIST_ROWS);

  wuss_window_get_content_bounds(ec->list_window, &bounds);
  if (bounds.x1 - bounds.x0 < EFFECTING_LIST_MIN_W)
  {
    sz = SIZE2D(EFFECTING_LIST_MIN_W, bounds.y1 - bounds.y0);
    (void) wuss_window_resize(ec->list_window, sz);
  }
}

/* Appends an element of the given type, selects it and re-runs. */
static result_t effecting_add(effecting_task_t *ec, effecting_type_t type)
{
  effecting_element_t el;

  memset(&el, 0, sizeof(el));
  el.type = type;

  switch (type)
  {
  case EFFECTING_CLEAR:
    el.u.clear = colour_rgb(0, 0, 0);
    break;

  case EFFECTING_TONE:
    el.u.tone = tonemap_create();
    if (el.u.tone == NULL)
      return result_OOM;
    break;

  case EFFECTING_BLUR:
    el.u.blur.method = effects_blur_GAUSSIAN;
    el.u.blur.amount = 8;
    break;

  default:
    break;
  }

  if (array_grow((void **) &ec->elements, sizeof(ec->elements[0]),
                 ec->nelements, &ec->allocated, 1, 8))
  {
    if (type == EFFECTING_TONE)
      tonemap_destroy(el.u.tone);

    return result_OOM;
  }

  ec->elements[ec->nelements++] = el;

  effecting_list_changed(ec);
  if (ec->list != NULL)
  {
    wuss_gridview_select(ec->list, ec->nelements - 1);
    effecting_fit_list(ec);
  }

  effecting_update(ec, 1);

  return result_OK;
}

static void effecting_free_element(effecting_element_t *el)
{
  if (el->type == EFFECTING_TONE)
    tonemap_destroy(el->u.tone);
}

static void effecting_delete(effecting_task_t *ec, int index)
{
  effecting_free_element(&ec->elements[index]);
  array_delete_element(ec->elements, sizeof(ec->elements[0]), ec->nelements,
                       index);
  ec->nelements--;

  if (ec->list != NULL)
    wuss_gridview_clear_selection(ec->list);
  effecting_list_changed(ec);
  effecting_update(ec, 1);
}

static void effecting_delete_all(effecting_task_t *ec)
{
  int i;

  for (i = 0; i < ec->nelements; i++)
    effecting_free_element(&ec->elements[i]);
  ec->nelements = 0;

  if (ec->list != NULL)
    wuss_gridview_clear_selection(ec->list);
  effecting_list_changed(ec);
  effecting_update(ec, 1);
}

/* Moves element from to position to, keeping it selected. */
static void effecting_move(effecting_task_t *ec, int from, int to)
{
  effecting_element_t el;

  if (from == to)
    return;

  el = ec->elements[from];
  if (from < to)
    memmove(&ec->elements[from], &ec->elements[from + 1],
            (size_t) (to - from) * sizeof(el));
  else
    memmove(&ec->elements[to + 1], &ec->elements[to],
            (size_t) (from - to) * sizeof(el));
  ec->elements[to] = el;

  if (ec->list != NULL)
  {
    effecting_list_changed(ec);
    wuss_gridview_select(ec->list, to);
  }

  effecting_update(ec, 1);
}

/* Makes the result the new source and empties the stack. */
static void effecting_apply(effecting_task_t *ec)
{
  if (ec->source.base == NULL)
    return;

  memcpy(ec->source.base, ec->result.base,
         (size_t) ec->source.rowbytes * ec->source.size.h);

  effecting_delete_all(ec);
  ec->level = 100;
  effecting_update(ec, 1);
}

/* Discards the stack and the level. */
static void effecting_cancel(effecting_task_t *ec)
{
  ec->level = 100;
  effecting_delete_all(ec);
}

/* wuss_gridview_drag_fn_t: opaque is the effecting_task_t */
static int effecting_drag(wuss_gridview_t *gridview, int index, void *opaque)
{
  effecting_task_t *ec;

  NOT_USED(gridview);

  ec             = opaque;
  ec->drag_index = index;

  return 1;
}

/* The drag ended: a drop on another row of the stack moves the element
 * there. */
static void effecting_drop(effecting_task_t *ec, const wuss_event_t *event)
{
  box_t   content;
  point_t scroll;
  point_t point;
  int     target;
  int     from;

  from           = ec->drag_index;
  ec->drag_index = -1;

  if (from < 0 || ec->list == NULL || event->data.drag_end.cancelled ||
      event->data.drag_end.drop != ec->list_window)
    return;

  wuss_window_get_content_bounds(ec->list_window, &content);
  wuss_window_get_scroll(ec->list_window, &scroll);

  point  = event->data.drag_end.point;
  point  = POINT(point.x - content.x0 + scroll.x,
                 point.y - content.y0 + scroll.y);
  target = wuss_gridview_hit_test(ec->list, point);
  if (target < 0)
    return;

  effecting_move(ec, from, target);
}

/* ----- files ----- */

/* wuss_saveas_save_fn_t: opaque is the effecting_task_t */
static result_t effecting_saveas_save(const char *path, void *opaque)
{
  effecting_task_t *ec;

  ec = opaque;
  if (ec->result.base == NULL)
    return result_BAD_ARG; /* nothing loaded yet; nothing to save */

  return bitmap_save_png(&ec->result, path);
}

/* Seed Save As with "<image>-effects.png". */
static void effecting_saveas_name(effecting_task_t *ec)
{
  char leafname[EFFECTING_MAX_NAME_LEN + 16];

  snprintf(leafname, sizeof(leafname), "%s-effects.png", ec->name);
  wuss_saveas_set_leafname(ec->saveas, leafname);
}

/* Load the PNG at path and re-apply, resizing the window to fit. A load
 * failure keeps the current image and is returned. */
static result_t effecting_show(effecting_task_t *ec, const char *path)
{
  result_t rc;
  bitmap_t next;
  char     name[EFFECTING_MAX_NAME_LEN];

  if (!path_leaf_strip_ext(path_leaf(path), ".png", name, sizeof(name)))
    return result_BAD_ARG;

  rc = load_png_deep(&next, path, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    logf_warning("effecting: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(ec->source.base);
  ec->source = next;
  strcpy(ec->name, name);
  effecting_saveas_name(ec);

  rc = effecting_rebuild(ec);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_resize(ec->window, ec->source.size);
  if (rc != result_OK)
    return rc;

  wuss_window_invalidate_visible(ec->window);
  return wuss_window_set_doc(ec->window, ec->source.size);
}

/* ----------------------------------------------------------------------- */

/* Builds the static menus; the Edit row and flags are set per open. */
static void effecting_build_menus(effecting_task_t *task)
{
  int i;

  for (i = 0; i < EFFECTING__LIMIT; i++)
    WUSS_MENU_ITEM(task->add_items, i, g_names[i], wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->add_menu, "Add", task->add_items,
                  NELEMS(task->add_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, EFFECTING_MENU_ADD, "Add",
                      wuss_MENU_ITEM_BORROWED_SUBMENU, &task->add_menu);
  WUSS_MENU_ITEM(task->menu_items, EFFECTING_MENU_EDIT, "Edit",
                 wuss_MENU_ITEM_DISABLED);
  WUSS_MENU_ITEM(task->menu_items, EFFECTING_MENU_DELETE, "Delete",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->menu_items, EFFECTING_MENU_DELETE_ALL, "Delete all",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM_WINDOW(task->menu_items, EFFECTING_MENU_LEVEL, "Level",
                        wuss_MENU_ITEM_BORROWED_SUBMENU |
                        wuss_MENU_ITEM_PRE_OPEN,
                        effecting_edit_window(task->edit,
                                              EFFECTING_EDIT_LEVEL));
  WUSS_MENU_ITEM(task->menu_items, EFFECTING_MENU_APPLY, "Apply",
                 wuss_MENU_ITEM_DASHED);
  WUSS_MENU_ITEM(task->menu_items, EFFECTING_MENU_CANCEL, "Cancel",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM_WINDOW(task->menu_items, EFFECTING_MENU_SAVE, "Save PNG",
                        wuss_MENU_ITEM_DASHED |
                        wuss_MENU_ITEM_BORROWED_SUBMENU |
                        wuss_MENU_ITEM_PRE_OPEN,
                        wuss_saveas_window(task->saveas));
  WUSS_MENU_TITLE(task->menu, "Effects", task->menu_items,
                  NELEMS(task->menu_items));
}

result_t effecting_create(wuss_t *wuss, effecting_task_t **out)
{
  result_t          rc;
  effecting_task_t *task;
  wuss_task_t      *delegate;
  wuss_task_desc_t  delegate_desc;
  wuss_place_t      place;
  size2d_t          sz;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss       = wuss;
  task->level      = 100;
  task->drag_index = -1;

  delegate_desc.handle    = effecting_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "effecting";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task);
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: QUIT is delivered at wuss_destroy instead. */

  sz = SIZE2D(EFFECTING_EMPTYSZ, EFFECTING_EMPTYSZ);

  rc = wuss_window_create_at(delegate,
                             &task_place,
                             sz,
                             "Effecting",
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

  rc = snapshot_saveas_create(&task->saveas, wuss, NULL, effecting_saveas_save,
                              task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  rc = effecting_edit_create(task, &task->edit);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  effecting_build_menus(task);

  /* the list's own handler routing is in effecting_handle; the window is
   * placed beside the image window */
  place = task_place_after(task->window);
  rc = wuss_gridview_window_create(&task->list, delegate, &place,
                                   "Effect stack", 0, effecting_item,
                                   EFFECTING_LABEL_CHARS, NULL, task,
                                   &task->list_window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate);
    return rc;
  }

  wuss_gridview_set_layout(task->list, 1, 0, 4);
  wuss_gridview_set_drag(task->list, effecting_drag);
  effecting_fit_list(task);

  if (out)
    *out = task;

  return result_OK;
}

result_t effecting_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Effecting",
    "Apply effects to PNGs",
    (taskapp_create_fn_t) effecting_create,
    (taskapp_data_load_fn_t) effecting_show,
    true
  };

  return taskapp_install(wuss, &desc);
}

void effecting_destroy(effecting_task_t *task)
{
  int i;

  for (i = 0; i < task->nelements; i++)
    effecting_free_element(&task->elements[i]);
  free(task->elements);

  wuss_gridview_destroy(task->list);
  effecting_edit_destroy(task->edit);
  wuss_saveas_destroy(task->saveas);
  free(task->source.base);
  free(task->chain.base);
  free(task->result.base);
  free(task);
}

/* ----------------------------------------------------------------------- */

static result_t effecting_redraw(const wuss_event_t *event, void *task_data)
{
  effecting_task_t *ec;
  screen_t         *scr;
  const box_t      *bounds;
  const bitmap_t   *bm;

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

static void effecting_menu_enable(effecting_task_t *ec, int row, int enabled)
{
  if (enabled)
    ec->menu_items[row].flags &=
      (wuss_menu_item_flags_t) ~wuss_MENU_ITEM_DISABLED;
  else
    ec->menu_items[row].flags |= wuss_MENU_ITEM_DISABLED;

  wuss_menu_disable_item_live(ec->menu_handle, &ec->menu, row, !enabled);
}

/* Sets the rows to match the selection and the stack. */
static void effecting_menu_update(effecting_task_t *ec)
{
  wuss_menu_item_t    *edit_row;
  effecting_element_t *el;

  el       = effecting_selected(ec, NULL);
  edit_row = &ec->menu_items[EFFECTING_MENU_EDIT];

  edit_row->submenu = NULL;
  edit_row->window  = NULL;
  edit_row->flags   = wuss_MENU_ITEM_DISABLED;
  if (el != NULL)
  {
    switch (el->type)
    {
    case EFFECTING_CLEAR:
      edit_row->submenu = wuss_colourmenu_menu(ec->wuss);
      edit_row->flags   = wuss_MENU_ITEM_PRE_OPEN;
      break;

    case EFFECTING_TONE:
      edit_row->window = effecting_edit_window(ec->edit, EFFECTING_EDIT_TONE);
      edit_row->flags  = wuss_MENU_ITEM_BORROWED_SUBMENU |
                         wuss_MENU_ITEM_PRE_OPEN;
      break;

    case EFFECTING_BLUR:
      edit_row->window = effecting_edit_window(ec->edit, EFFECTING_EDIT_BLUR);
      edit_row->flags  = wuss_MENU_ITEM_BORROWED_SUBMENU |
                         wuss_MENU_ITEM_PRE_OPEN;
      break;

    default:
      break;
    }
  }

  wuss_menu_disable_item_live(ec->menu_handle, &ec->menu, EFFECTING_MENU_EDIT,
                              edit_row->flags & wuss_MENU_ITEM_DISABLED);
  effecting_menu_enable(ec, EFFECTING_MENU_DELETE, el != NULL);
  effecting_menu_enable(ec, EFFECTING_MENU_DELETE_ALL, ec->nelements > 0);
  effecting_menu_enable(ec, EFFECTING_MENU_APPLY, ec->nelements > 0);
  effecting_menu_enable(ec, EFFECTING_MENU_CANCEL, ec->nelements > 0);
}

static result_t effecting_open_menu(effecting_task_t *ec)
{
  effecting_menu_update(ec);

  return wuss_menu_open_at_pointer(ec->delegate, &ec->menu, &ec->menu_handle);
}

static result_t effecting_menu_select(effecting_task_t   *ec,
                                      const wuss_event_t *event)
{
  const wuss_menu_t   *menu;
  effecting_element_t *el;
  colour_t             colour;
  int                  index;
  int                  selected;

  menu  = event->data.menu_select.menu;
  index = event->data.menu_select.index;

  if (wuss_colourmenu_selected_rgb(event, &colour))
  {
    el = effecting_selected(ec, NULL);
    if (el != NULL && el->type == EFFECTING_CLEAR)
    {
      el->u.clear = colour;
      effecting_update(ec, 1);
      if (ec->list != NULL)
        (void) wuss_gridview_set_count(ec->list, ec->nelements);
    }

    return result_OK;
  }

  if (menu == &ec->add_menu)
    return effecting_add(ec, (effecting_type_t) index);

  if (menu != &ec->menu)
    return result_OK;

  switch (index)
  {
  case EFFECTING_MENU_DELETE:
    if (effecting_selected(ec, &selected) != NULL)
      effecting_delete(ec, selected);
    return result_OK;

  case EFFECTING_MENU_DELETE_ALL:
    effecting_delete_all(ec);
    return result_OK;

  case EFFECTING_MENU_APPLY:
    effecting_apply(ec);
    return result_OK;

  case EFFECTING_MENU_CANCEL:
    effecting_cancel(ec);
    return result_OK;

  case EFFECTING_MENU_SAVE:
    return wuss_saveas_open(ec->saveas); /* a click, not a hover */

  default:
    return result_OK;
  }
}

result_t effecting_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  result_t          rc;
  effecting_task_t *ec;
  int               which;

  ec = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    if (window == ec->list_window && ec->list != NULL)
    {
      (void) wuss_gridview_handle_event(ec->list, event);
      return result_OK;
    }
    return effecting_redraw(event, task_data);

  case wuss_EVENT_OPEN:
  case wuss_EVENT_PALETTE:
  case wuss_EVENT_POINTER_EXIT:
  case wuss_EVENT_RELAYOUT:
    if (window == ec->list_window && ec->list != NULL)
      (void) wuss_gridview_handle_event(ec->list, event);
    return result_OK;

  case wuss_EVENT_MOUSE:
    if (window == ec->list_window && ec->list != NULL)
    {
      (void) wuss_gridview_handle_event(ec->list, event);
      if (event->data.mouse.action == wuss_MOUSE_DOWN &&
          (event->data.mouse.button & wuss_BUTTON_MENU))
        return effecting_open_menu(ec);
      return result_OK;
    }
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
      return effecting_open_menu(ec);
    return result_OK;

  case wuss_EVENT_DRAG_END:
    effecting_drop(ec, event);
    return result_OK;

  case wuss_EVENT_ICON:
    which = effecting_edit_find(ec->edit, window);
    if (which < 0)
      return result_OK;
    return effecting_edit_icon(ec->edit, (effecting_edit_which_t) which,
                               event);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
  {
    effecting_element_t *el;

    /* the Edit row of a Clear element: the colour menu */
    el = effecting_selected(ec, NULL);
    return wuss_colourmenu_open_rgb(ec->wuss, event, "Clear colour",
                                    el != NULL && el->type == EFFECTING_CLEAR ?
                                    el->u.clear : colour_rgb(0, 0, 0));
  }

  case wuss_EVENT_MENU_SELECT:
    return effecting_menu_select(ec, event);

  case wuss_EVENT_MESSAGE:
    if (ec->window == NULL || window != ec->window)
      return result_OK;
    return taskapp_window_data_load(ec->wuss, ec->delegate, ec,
                                    (taskapp_data_load_fn_t) effecting_show,
                                    event->data.message);

  case wuss_EVENT_MENU_CLOSED:
    ec->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == ec->window)
    {
      ec->window = NULL;
    }
    else if (window == ec->list_window)
    {
      wuss_gridview_destroy(ec->list);
      ec->list        = NULL;
      ec->list_window = NULL;
    }
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (event->data.pre_show.handle == NULL)
      return result_OK;

    which = effecting_edit_find(ec->edit, window);
    if (which >= 0)
    {
      rc = effecting_edit_pre_show(ec->edit, (effecting_edit_which_t) which);
      if (rc != result_OK)
        return rc;
    }
    else if (window != wuss_saveas_window(ec->saveas))
    {
      return result_OK;
    }

    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);

  case wuss_EVENT_QUIT:
    effecting_destroy(ec);
    return result_OK;

  case wuss_EVENT_KEY:
    if (window == ec->list_window &&
        event->data.key.code == wuss_KEY_DELETE)
    {
      int selected;

      if (effecting_selected(ec, &selected) != NULL)
        effecting_delete(ec, selected);
      return result_OK;
    }
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
