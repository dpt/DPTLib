/* wuss/component/colourmenu.c -- a menu of the system palette colours */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

#include "wuss/component/colourmenu.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list, one item per palette entry plus a trailing "None"
 * row (always built, shown/hidden via g.npalette/g.menu->nitems), index
 * order. One process-wide singleton -- no per-task instances, no
 * create/destroy -- rebuilt lazily when the caller's wuss_t or its palette
 * size changes. */
static struct
{
  wuss_alloc_t      alloc;
  const wuss_t     *wuss;      /* whose palette menu was built from */
  int               npalette;  /* palette rows in menu, None excluded */
  wuss_menu_t      *menu;
  wuss_menu_item_t *items;
  char             *title;
  int               with_none; /* survives rebuild; caller's last
                                * wuss_colourmenu_set_none call */
}
g = { .with_none = 1 };

/* Free the singleton's owned blocks -- items, their text, title, itself --
 * leaving g.menu/g.wuss cleared so the next call rebuilds from scratch. */
static void colourmenu_free(void)
{
  int i;

  if (g.menu == NULL)
    return; /* never built, or already freed -- g.alloc may be unset */

  if (g.items != NULL)
  {
    for (i = 0; i < g.npalette + 1; i++)
      g.alloc.free((void *) g.items[i].text); /* discard const */
    g.alloc.free(g.items);
    g.items = NULL;
  }
  g.alloc.free(g.title);
  g.title = NULL;
  g.alloc.free(g.menu);
  g.menu     = NULL;
  g.wuss     = NULL;
  g.npalette = 0;
}

/* (Re)build the singleton against wuss's current palette. */
static result_t colourmenu_build(const wuss_t *wuss)
{
  result_t rc;
  int      npalette;
  int      i;

  colourmenu_free();

  g.alloc = wuss->alloc;

  npalette = wuss->npalette;
  if (npalette > wuss_COLOUR_SYMBOLIC)
    npalette = wuss_COLOUR_SYMBOLIC; /* wuss_colour_t indices above this are
                                      * the symbolic/chrome-role namespace,
                                      * not real palette slots -- don't hand
                                      * them out as swatches */

  g.menu = g.alloc.malloc(sizeof(*g.menu));
  if (g.menu == NULL)
    return result_OOM;

  g.menu->title  = NULL;
  g.menu->items  = NULL;
  g.menu->nitems = 0;

  g.items = g.alloc.malloc((size_t) (npalette + 1) * sizeof(*g.items));
  if (g.items == NULL)
  {
    rc = result_OOM;
    goto failure;
  }
  memset(g.items, 0, (size_t) (npalette + 1) * sizeof(*g.items));
  g.menu->items = g.items;

  g.title = wuss__alloc_strdup(&g.alloc, "Colour");
  if (g.title == NULL)
  {
    rc = result_OOM;
    goto failure;
  }
  g.menu->title = g.title;

  for (i = 0; i < npalette; i++)
  {
    unsigned int r, gr, b;
    char         label[8];

    colour_get_rgb(&wuss->palette[i], &r, &gr, &b);
    snprintf(label, sizeof(label), "#%02X%02X%02X", r, gr, b);

    g.items[i].text = wuss__alloc_strdup(&g.alloc, label);
    if (g.items[i].text == NULL)
    {
      rc = result_OOM;
      goto failure;
    }

    g.items[i].flags  = wuss_MENU_ITEM_SWATCH;
    g.items[i].swatch = (wuss_colour_t) i;
  }

  g.items[npalette].text   = wuss__alloc_strdup(&g.alloc, "None");
  g.items[npalette].flags  = wuss_MENU_ITEM_SWATCH | wuss_MENU_ITEM_DASHED;
  g.items[npalette].swatch = wuss_NO_BACKGROUND;
  if (g.items[npalette].text == NULL)
  {
    rc = result_OOM;
    goto failure;
  }

  g.npalette     = npalette;
  g.menu->nitems = g.with_none ? npalette + 1 : npalette;
  g.wuss         = wuss;

  return result_OK;

failure:
  colourmenu_free();
  return rc;
}

const wuss_menu_t *wuss_colourmenu_menu(const wuss_t *wuss)
{
  if (wuss == NULL)
    return NULL;

  if (wuss != g.wuss)
    if (colourmenu_build(wuss) != result_OK)
      return NULL;

  return g.menu;
}

void wuss_colourmenu_set_none(int with_none)
{
  g.with_none = with_none;

  if (g.menu == NULL)
    return;

  g.menu->nitems = with_none ? g.npalette + 1 : g.npalette;
}

result_t wuss_colourmenu_set_title(const char *title)
{
  char *copy;

  if (g.menu == NULL)
    return result_NULL_ARG;

  copy = wuss__alloc_strdup(&g.alloc, title ? title : "Colour");
  if (copy == NULL)
    return result_OOM;

  g.alloc.free(g.title);
  g.title       = copy;
  g.menu->title = g.title;

  return result_OK;
}

wuss_colour_t wuss_colourmenu_selected(const wuss_event_t *ev, int *ok)
{
  int index;

  if (ok != NULL)
    *ok = 0;

  if (ev == NULL || g.menu == NULL)
    return 0;
  if (ev->kind != wuss_EVENT_MENU_SELECT)
    return 0;
  if (ev->data.menu_select.menu != g.menu)
    return 0;

  index = ev->data.menu_select.index;
  if (index < 0 || index >= g.menu->nitems)
    return 0;

  if (ok != NULL)
    *ok = 1;
  return g.menu->items[index].swatch;
}
