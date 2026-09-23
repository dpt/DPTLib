/* wuss/gadget/stringset.c -- a field with a pop-up menu of fixed strings */

#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "geom/box.h"
#include "geom/point.h"

#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "wuss/gadget/stringset.h"

#include "../core/impl.h"
#include "../icon.h"

/* ----------------------------------------------------------------------- */

/* The menu's items[] trails the struct, one per string, so the whole gadget
 * is a single allocation. handle is non-NULL only while the gadget's chain
 * is open: a closing pick or a wuss_EVENT_MENU_CLOSED drops it, since the
 * chain is freed by then and wuss_menu_close would dereference it. */
struct wuss_stringset
{
  wuss_alloc_t                 alloc;  /* copied hooks; wuss_t not retained */
  wuss_window_t               *window;
  wuss_icon_t                 *field;
  wuss_icon_t                 *arrow;
  const char *const           *strings;
  int                          count;
  int                          index;
  wuss_stringset_changed_fn_t *changed;
  void                        *opaque;
  wuss_menu_handle_t           handle;
  wuss_menu_t                  menu;
  wuss_menu_item_t             items[];
};

/* ----------------------------------------------------------------------- */

/* Fill "spec" as the arrow at the right edge of "bbox", vertically centred,
 * returning its width. */
static int stringset_arrow_spec(const wuss_t     *wuss,
                                box_t             bbox,
                                wuss_icon_spec_t *spec)
{
  const bitmap_t *bm;
  int             idx, pidx;
  int             w, h;

  idx = wuss_icons_lookup(wuss, "gright");
  bm  = (idx >= 0) ? wuss_icons_bitmap(wuss, idx) : NULL;
  if (bm == NULL)
  {
    /* no icon set: a square button the height of the gadget */
    h = bbox.y1 - bbox.y0;
    wuss_icon_spec_action(spec,
                          (box_t) BOX_POS_SIZE(bbox.x1 - h, bbox.y0, h, h),
                          ">", 0);
    return h;
  }

  w    = bm->size.w;
  h    = bm->size.h;
  pidx = wuss_icons_lookup(wuss, "gright-p");

  memset(spec, 0, sizeof(*spec));
  spec->bbox  = (box_t) BOX_POS_SIZE(bbox.x1 - w,
                                     bbox.y0 + (bbox.y1 - bbox.y0 - h) / 2,
                                     w, h);
  spec->type  = wuss_ICON_TYPE_BITMAP;
  spec->flags = wuss_ICON_FLAGS_INTERACTIVE;
  spec->u.bitmap.set         = wuss_ICON_SET(idx);
  spec->u.bitmap.pressed_set = (pidx >= 0) ? wuss_ICON_SET(pidx) : 0;
  return w;
}

result_t wuss_stringset_create(wuss_stringset_t           **out,
                               wuss_window_t               *window,
                               box_t                        bbox,
                               const char                  *title,
                               const char *const           *strings,
                               int                          count,
                               wuss_stringset_changed_fn_t *changed,
                               void                        *opaque)
{
  result_t          rc;
  wuss_t           *wuss;
  wuss_stringset_t *ss;
  wuss_icon_spec_t  specs[2];
  wuss_icon_t      *icons[2];
  box_t             field;
  int               i;

  if (out == NULL || window == NULL || strings == NULL)
    return result_NULL_ARG;

  if (count < 1)
    return result_BAD_ARG;

  wuss = window->task->wuss;

  ss = wuss->alloc.malloc(sizeof(*ss) + (size_t) count * sizeof(ss->items[0]));
  if (ss == NULL)
    return result_OOM;

  memset(ss->items, 0, (size_t) count * sizeof(ss->items[0]));
  for (i = 0; i < count; i++)
    WUSS_MENU_ITEM(ss->items, i, strings[i], wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(ss->menu, title, ss->items, count);

  field     = bbox;
  field.x1 -= stringset_arrow_spec(wuss, bbox, &specs[1]) + wuss_STD_GAP;
  wuss_icon_spec_display(&specs[0], field, strings[0], 0);

  rc = wuss_icon_create_array(window, specs, 2, icons);
  if (rc != result_OK)
  {
    wuss->alloc.free(ss);
    return rc;
  }

  ss->alloc   = wuss->alloc;
  ss->window  = window;
  ss->field   = icons[0];
  ss->arrow   = icons[1];
  ss->strings = strings;
  ss->count   = count;
  ss->index   = 0;
  ss->changed = changed;
  ss->opaque  = opaque;
  ss->handle  = NULL;

  *out = ss;
  return result_OK;
}

void wuss_stringset_destroy(wuss_stringset_t *doomed)
{
  if (doomed == NULL)
    return;

  wuss_menu_close(doomed->handle);
  doomed->alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

/* Open the menu with its top-left at the arrow's top-right, current entry
 * ticked. */
static result_t stringset_open(wuss_stringset_t *ss)
{
  box_t   content, bbox, screen;
  point_t scroll;

  wuss_window_get_content_bounds(ss->window, &content);
  wuss_window_get_scroll(ss->window, &scroll);
  wuss_icon_get_bbox(ss->arrow, &bbox);
  wuss__icon_box_to_screen(&content, scroll, &bbox, &screen);

  wuss_menu_tick_exclusive(&ss->menu, ss->index);
  return wuss_menu_open(ss->window->task, &ss->menu,
                        POINT(screen.x1, screen.y0), &ss->handle);
}

int wuss_stringset_handle_event(wuss_stringset_t   *ss,
                                const wuss_event_t *event,
                                result_t           *out_result)
{
  result_t rc;
  int      index;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    if (event->data.icon.icon != ss->arrow)
      return 0;

    rc = result_OK;
    if (event->data.icon.action == wuss_MOUSE_UP &&
        (event->data.icon.button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      rc = stringset_open(ss);
    *out_result = rc;
    return 1;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu != &ss->menu)
      return 0;

    index = event->data.menu_select.index;
    rc    = result_OK;
    if (index >= 0 && index < ss->count && index != ss->index)
    {
      rc = wuss_stringset_set_index(ss, index);
      if (rc == result_OK && ss->changed != NULL)
        rc = ss->changed(ss, index, ss->opaque);
    }

    /* an Adjust pick leaves the menu open: move its tick too */
    if (wuss_menu_should_keep_open(event))
      wuss_menu_tick_exclusive_live(ss->handle, &ss->menu, ss->index);
    else
      ss->handle = NULL;

    *out_result = rc;
    return 1;

  case wuss_EVENT_MENU_CLOSED:
    /* carries no menu, but only one chain is ever open: if ours was live it
     * is the one that closed. Not consumed -- the task may hold a handle of
     * its own to drop. */
    ss->handle = NULL;
    return 0;

  default:
    return 0;
  }
}

/* ----------------------------------------------------------------------- */

int wuss_stringset_get_index(const wuss_stringset_t *ss)
{
  return ss->index;
}

result_t wuss_stringset_set_index(wuss_stringset_t *ss, int index)
{
  result_t rc;

  if (index < 0 || index >= ss->count)
    return result_BAD_ARG;

  rc = wuss_icon_set_text(ss->window, ss->field, ss->strings[index]);
  if (rc != result_OK)
    return rc;

  ss->index = index;
  return result_OK;
}
