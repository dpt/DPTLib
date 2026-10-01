/* wuss/gadget/colourset.c -- a titled colour well with a colour menu */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "geom/box.h"
#include "geom/point.h"

#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "wuss/component/colourmenu.h"
#include "wuss/gadget/colourset.h"

#include "../core/impl.h"
#include "../font/font.h"
#include "../icon.h"

/* ----------------------------------------------------------------------- */

/* handle is non-NULL only while the gadget's chain is open: a
 * wuss_EVENT_MENU_CLOSED drops it, since the chain is freed by then and
 * wuss_menu_close would dereference it. It also tells the gadget's picks
 * apart from another caller's use of the shared colour menu. A SELECT pick
 * closes the chain before its wuss_EVENT_MENU_SELECT arrives, so picking
 * carries ownership across that gap. */
struct wuss_colourset
{
  wuss_alloc_t                 alloc;  /* copied hooks; wuss_t not retained */
  wuss_window_t               *window;
  wuss_icon_t                 *field;
  wuss_icon_t                 *button;
  const char                  *title;
  wuss_colour_t                colour;
  wuss_colourset_changed_fn_t *changed;
  void                        *opaque;
  wuss_menu_handle_t           handle;
  int                          picking; /* a closing pick is in flight */
};

/* ----------------------------------------------------------------------- */

/* Fill "spec" as the button at the right edge of "bbox", vertically centred,
 * returning its width. */
static int colourset_button_spec(const wuss_t     *wuss,
                                 box_t             bbox,
                                 wuss_icon_spec_t *spec)
{
  const bitmap_t *bm;
  int             idx, pidx;
  int             w, h;

  idx = wuss_icons_lookup(wuss, "grightc");
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
  pidx = wuss_icons_lookup(wuss, "grightc-p");

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

/* The concrete palette index for "colour", or -1 if it is
 * wuss_NO_BACKGROUND or out of range. */
static int colourset_resolve(const wuss_t *wuss, wuss_colour_t colour)
{
  if (colour == wuss_NO_BACKGROUND)
    return -1;

  colour = wuss__resolve_colour(wuss, colour);
  return (colour < wuss->npalette) ? colour : -1;
}

result_t wuss_colourset_create(wuss_colourset_t           **out,
                               wuss_window_t               *window,
                               box_t                        bbox,
                               const char                  *title,
                               int                          title_width,
                               wuss_colour_t                colour,
                               wuss_colourset_changed_fn_t *changed,
                               void                        *opaque)
{
  result_t          rc;
  wuss_t           *wuss;
  int               resolved;
  wuss_colourset_t *cs;
  wuss_icon_spec_t  specs[3];
  wuss_icon_t      *icons[3];
  box_t             field;
  bmfont_t         *font;
  bmfont_width_t    w;
  int               nspecs;

  if (out == NULL || window == NULL)
    return result_NULL_ARG;

  wuss = window->task->wuss;

  resolved = colourset_resolve(wuss, colour);
  if (resolved < 0)
    return result_WUSS_BAD_COLOUR;

  cs = wuss->alloc.malloc(sizeof(*cs));
  if (cs == NULL)
    return result_OOM;

  field     = bbox;
  field.x1 -= colourset_button_spec(wuss, bbox, &specs[1]) + wuss_STD_GAP;
  nspecs    = 2;

  font = wuss->fonts.fonts[0];
  if (title != NULL && title[0] != '\0' && font != NULL)
  {
    w = (bmfont_width_t) title_width;
    if (title_width <= 0)
    {
      wuss__text_measure(font, title, (int) strlen(title), INT_MAX, NULL, &w);
      w += 2 * WUSS_LABEL_TEXT_PAD;
    }
    wuss_icon_spec_label(&specs[2],
                         (box_t) BOX_POS_SIZE(bbox.x0, bbox.y0, (int) w,
                                              bbox.y1 - bbox.y0),
                         title, wuss_ICON_FLAGS_JUSTIFY_RIGHT);
    field.x0 += (int) w + wuss_STD_GAP;
    nspecs    = 3;
  }

  wuss_icon_spec_display(&specs[0], field, NULL, 0);
  specs[0].bg = (wuss_colour_t) resolved;

  rc = wuss_icon_create_array(window, specs, nspecs, icons);
  if (rc != result_OK)
  {
    wuss->alloc.free(cs);
    return rc;
  }

  cs->alloc   = wuss->alloc;
  cs->window  = window;
  cs->field   = icons[0];
  cs->button  = icons[1];
  cs->title   = title;
  cs->colour  = (wuss_colour_t) resolved;
  cs->changed = changed;
  cs->opaque  = opaque;
  cs->handle  = NULL;
  cs->picking = 0;

  *out = cs;
  return result_OK;
}

int wuss_colourset_button_width(const wuss_t *wuss, int height)
{
  wuss_icon_spec_t spec;

  return colourset_button_spec(wuss, (box_t) BOX_POS_SIZE(0, 0, height, height),
                               &spec);
}

void wuss_colourset_destroy(wuss_colourset_t *doomed)
{
  if (doomed == NULL)
    return;

  wuss_menu_close(doomed->handle);
  doomed->alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

/* Open the colour menu with its top-left at the button's top-right, set up
 * for this gadget: retitled, None hidden, current colour ticked. */
static result_t colourset_open(wuss_colourset_t *cs)
{
  const wuss_menu_t *menu;
  box_t              content, bbox, screen;
  point_t            scroll;

  menu = wuss_colourmenu_menu(cs->window->task->wuss);
  if (menu == NULL)
    return result_OOM;

  wuss_colourmenu_set_none(0);
  (void) wuss_colourmenu_set_title(cs->title); /* ponytail: OOM keeps the
                                                * old title */
  wuss_colourmenu_set_ticked(cs->colour);

  wuss_window_get_content_bounds(cs->window, &content);
  wuss_window_get_scroll(cs->window, &scroll);
  wuss_icon_get_bbox(cs->button, &bbox);
  wuss__icon_box_to_screen(&content, scroll, &bbox, &screen);

  return wuss_menu_open(cs->window->task, menu,
                        POINT(screen.x1, screen.y0), &cs->handle);
}

int wuss_colourset_handle_event(wuss_colourset_t   *cs,
                                const wuss_event_t *event,
                                result_t           *out_result)
{
  result_t      rc;
  wuss_colour_t colour;
  int           ok;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    if (event->data.icon.icon != cs->button)
      return 0;

    rc = result_OK;
    if (event->data.icon.action == wuss_MOUSE_UP &&
        (event->data.icon.button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      rc = colourset_open(cs);
    *out_result = rc;
    return 1;

  case wuss_EVENT_MENU_SELECT:
    if (cs->handle == NULL && !cs->picking)
      return 0; /* not our chain: someone else's use of the colour menu */

    cs->picking = 0;

    colour = wuss_colourmenu_selected(event, &ok); /* moves the tick too */
    if (!ok)
      return 0;

    rc = result_OK;
    if (colour != wuss_NO_BACKGROUND && colour != cs->colour)
    {
      rc = wuss_colourset_set_colour(cs, colour);
      if (rc == result_OK && cs->changed != NULL)
        rc = cs->changed(cs, colour, cs->opaque);
    }

    *out_result = rc;
    return 1;

  case wuss_EVENT_MENU_CLOSED:
    /* carries no menu, but only one chain is ever open: if ours was live it
     * is the one that closed. Not consumed -- the task may hold a handle of
     * its own to drop. */
    cs->picking = cs->handle != NULL && event->data.menu_closed.picked;
    cs->handle  = NULL;
    return 0;

  default:
    return 0;
  }
}

/* ----------------------------------------------------------------------- */

wuss_colour_t wuss_colourset_get_colour(const wuss_colourset_t *cs)
{
  return cs->colour;
}

result_t wuss_colourset_set_colour(wuss_colourset_t *cs,
                                   wuss_colour_t     colour)
{
  int resolved;

  resolved = colourset_resolve(cs->window->task->wuss, colour);
  if (resolved < 0)
    return result_WUSS_BAD_COLOUR;

  cs->colour         = (wuss_colour_t) resolved;
  cs->field->spec.bg = cs->colour; /* no public setter: this is its only
                                    * user */
  wuss__icon_invalidate(cs->window, cs->field);
  return result_OK;
}
