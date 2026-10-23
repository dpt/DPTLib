/* wuss/gadget/colourset.c -- a titled colour well with a colour menu */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "geom/point.h"

#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#include "wuss/component/colourmenu.h"
#include "wuss/gadget/colourset.h"

#include "../core/types.h"
#include "../core/palette.h"
#include "../font/font.h"
#include "../icon.h"
#include "popup.h"

/* ----------------------------------------------------------------------- */

/* handle is non-NULL only while the gadget's chain is open: a
 * wuss_EVENT_MENU_CLOSED drops it, since the chain is freed by then and
 * wuss_menu_close would dereference it. It also tells the gadget's picks
 * apart from another caller's use of the shared colour menu. A SELECT pick
 * closes the chain before its wuss_EVENT_MENU_SELECT arrives, so picking
 * carries ownership across that gap.
 *
 * The field is a grooved bitmap icon over "bm", so it shows "rgb" exactly
 * rather than snapped to the desktop palette. "colour" is the palette entry
 * nearest "rgb", for the menu's tick and wuss_colourset_get_colour. */
struct wuss_colourset
{
  wuss_alloc_t                 alloc;  /* copied hooks; wuss_t not retained */
  wuss_window_t               *window;
  wuss_icon_t                 *field;
  wuss_icon_t                 *button;
  const char                  *title;
  wuss_colour_t                colour;
  colour_t                     rgb;     /* what the field shows */
  int                          indexed; /* rgb is palette[colour]: follows
                                         * wuss_EVENT_PALETTE */
  bitmap_t                     bm;
  void                        *pixels;
  wuss_colourset_changed_fn_t *changed;
  void                        *opaque;
  wuss_menu_handle_t           handle;
  int                          picking; /* a closing pick is in flight */
};

/* ----------------------------------------------------------------------- */

/* fill the field with "rgb" */
static void colourset_show(wuss_colourset_t *cs, colour_t rgb)
{
  cs->rgb = rgb;
  bitmap_clear(&cs->bm, rgb);
  wuss__icon_invalidate(cs->window, cs->field);
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
  int               nspecs;
  size2d_t          size;

  if (out == NULL || window == NULL)
    return result_NULL_ARG;

  wuss = window->task->wuss;

  resolved = colourset_resolve(wuss, colour);
  if (resolved < 0)
    return result_WUSS_BAD_COLOUR;

  cs = wuss->alloc.malloc(sizeof(*cs));
  if (cs == NULL)
    return result_OOM;

  nspecs = wuss__popup_layout(wuss, bbox, title, title_width, "grightc",
                              "grightc-p", specs, &field);

  /* the image fills the groove's interior */
  size.w     = MAX(field.x1 - field.x0 - 2 * WUSS_BEVEL_WIDTH, 1);
  size.h     = MAX(field.y1 - field.y0 - 2 * WUSS_BEVEL_WIDTH, 1);
  cs->pixels = wuss->alloc.malloc((size_t) size.w * size.h * 4);
  if (cs->pixels == NULL)
  {
    wuss->alloc.free(cs);
    return result_OOM;
  }

  rc = bitmap_init(&cs->bm, size, pixelfmt_bgrx8888, size.w * 4, NULL,
                   cs->pixels);
  if (rc != result_OK)
    goto failure;

  memset(&specs[0], 0, sizeof(specs[0]));
  specs[0].bbox            = field;
  specs[0].type            = wuss_ICON_TYPE_BITMAP;
  specs[0].bg              = wuss_NO_BACKGROUND;
  specs[0].u.bitmap.image  = &cs->bm;
  specs[0].u.bitmap.border = wuss_ICON_BORDER_GROOVE;

  rc = wuss_icon_create_array(window, specs, nspecs, icons);
  if (rc != result_OK)
    goto failure;

  cs->alloc   = wuss->alloc;
  cs->window  = window;
  cs->field   = icons[0];
  cs->button  = icons[1];
  cs->title   = title;
  cs->colour  = (wuss_colour_t) resolved;
  cs->indexed = 1;
  cs->changed = changed;
  cs->opaque  = opaque;
  cs->handle  = NULL;
  cs->picking = 0;

  colourset_show(cs, wuss->palette[resolved]);

  *out = cs;
  return result_OK;

failure:
  wuss->alloc.free(cs->pixels);
  wuss->alloc.free(cs);
  return rc;
}

int wuss_colourset_button_width(const wuss_t *wuss, int height)
{
  wuss_icon_spec_t spec;

  return wuss__popup_arrow_spec(wuss,
                                (box_t) BOX_POS_SIZE(0, 0, height, height),
                                "grightc", "grightc-p", &spec);
}

void wuss_colourset_destroy(wuss_colourset_t *doomed)
{
  if (doomed == NULL)
    return;

  wuss_menu_close(doomed->handle);
  doomed->alloc.free(doomed->pixels);
  doomed->alloc.free(doomed);
}

/* ----------------------------------------------------------------------- */

/* Open the colour menu with its top-left at the button's top-right, set up
 * for this gadget: retitled, None hidden, current colour ticked -- or, for
 * an RGB no entry matches, nothing visibly ticked (the hidden None row). */
static result_t colourset_open(wuss_colourset_t *cs)
{
  const wuss_t      *wuss;
  const wuss_menu_t *menu;

  wuss = cs->window->task->wuss;
  menu = wuss_colourmenu_menu(wuss);
  if (menu == NULL)
    return result_OOM;

  wuss_colourmenu_set_none(0);
  (void) wuss_colourmenu_set_title(cs->title); /* ponytail: OOM keeps the
                                                * old title */
  wuss_colourmenu_set_ticked(wuss->palette[cs->colour].primary ==
                             cs->rgb.primary ? cs->colour
                                             : wuss_NO_BACKGROUND);

  return wuss__popup_open(cs->window, cs->button, menu, &cs->handle);
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
        (event->data.icon.button & WUSS_BUTTON_CLICK))
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

    /* a pick of the nearest entry to an unmatched RGB is still a change */
    rc = result_OK;
    if (colour != wuss_NO_BACKGROUND &&
        (colour != cs->colour || !cs->indexed))
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

  case wuss_EVENT_PALETTE:
    /* not consumed: every gadget and the task itself may want it */
    if (cs->indexed)
      colourset_show(cs, cs->window->task->wuss->palette[cs->colour]);
    else
      wuss_colourset_set_rgb(cs, cs->rgb); /* re-find the nearest entry */
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
  const wuss_t *wuss;
  int           resolved;

  wuss = cs->window->task->wuss;

  resolved = colourset_resolve(wuss, colour);
  if (resolved < 0)
    return result_WUSS_BAD_COLOUR;

  cs->colour  = (wuss_colour_t) resolved;
  cs->indexed = 1;
  colourset_show(cs, wuss->palette[resolved]);
  return result_OK;
}

void wuss_colourset_set_rgb(wuss_colourset_t *cs, colour_t rgb)
{
  unsigned int r, g, b;

  colour_get_rgb(&rgb, &r, &g, &b);
  cs->colour  = wuss_nearest_colour(cs->window->task->wuss, (int) r, (int) g,
                                    (int) b);
  cs->indexed = 0;
  colourset_show(cs, rgb);
}
