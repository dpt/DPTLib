/* wuss/component/colourmenu.c -- a menu of the system palette colours */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "wuss/menu.h"
#include "wuss/task.h"

#include "wuss/component/colourmenu.h"

#include "../impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list, one item per palette entry, index order. No grouping
 * of near colours, no "recent" section. wuss_colourmenu_selected is then a
 * single pointer compare plus the item index. */
struct wuss_colourmenu
{
  wuss_menu_t *menu; /* owned; freed by wuss_menu_destroy */
};

result_t wuss_colourmenu_create(wuss_colourmenu_t **out,
                                const wuss_t       *wuss,
                                const char         *title)
{
  wuss_colourmenu_t *cm;
  wuss_menu_t       *m;
  wuss_menu_item_t  *items;
  int                n;
  int                i;

  if (out == NULL || wuss == NULL)
    return result_NULL_ARG;

  n = wuss->npalette;

  cm = malloc(sizeof(*cm));
  if (cm == NULL)
    return result_OOM;

  m = calloc(1, sizeof(*m));
  if (m == NULL)
  {
    free(cm);
    return result_OOM;
  }

  items = (n > 0) ? calloc((size_t) n, sizeof(*items)) : NULL;
  if (n > 0 && items == NULL)
  {
    free(m);
    free(cm);
    return result_OOM;
  }

  m->title  = strdup(title ? title : "Colour");
  m->items  = items;
  m->nitems = n; /* items[i].text/submenu all NULL: safe for wuss_menu_destroy */
  if (m->title == NULL)
  {
    wuss_menu_destroy(m);
    free(cm);
    return result_OOM;
  }

  for (i = 0; i < n; i++)
  {
    pixelfmt_rgba8888_t px;
    char                label[8];

    px = wuss->palette[i].primary;
    snprintf(label, sizeof(label), "#%02X%02X%02X",
             PIXELFMT_Rxxx8888(px),
             PIXELFMT_xGxx8888(px),
             PIXELFMT_xxBx8888(px));

    items[i].text = strdup(label);
    if (items[i].text == NULL)
    {
      wuss_menu_destroy(m);
      free(cm);
      return result_OOM;
    }

    items[i].flags  = wuss_MENU_ITEM_SWATCH;
    items[i].swatch = (wuss_colour_t) i;
  }

  cm->menu = m;
  *out = cm;
  return result_OK;
}

void wuss_colourmenu_destroy(wuss_colourmenu_t *doomed)
{
  if (doomed == NULL)
    return;

  wuss_menu_destroy(doomed->menu);
  free(doomed);
}

const wuss_menu_t *wuss_colourmenu_menu(const wuss_colourmenu_t *cm)
{
  return cm ? cm->menu : NULL;
}

wuss_colour_t wuss_colourmenu_selected(const wuss_colourmenu_t *cm,
                                       const wuss_event_t      *ev,
                                       int                     *ok)
{
  int index;

  if (ok != NULL)
    *ok = 0;

  if (cm == NULL || ev == NULL)
    return 0;
  if (ev->kind != wuss_EVENT_MENU_SELECT)
    return 0;
  if (ev->data.menu_select.menu != cm->menu)
    return 0;

  index = ev->data.menu_select.index;
  if (index < 0 || index >= cm->menu->nitems)
    return 0;

  if (ok != NULL)
    *ok = 1;
  return cm->menu->items[index].swatch;
}
