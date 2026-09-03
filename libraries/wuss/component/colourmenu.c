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

#include "wuss/component/colourmenu.h"

#include "../impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list, one item per palette entry, index order. No grouping
 * of near colours, no "recent" section. wuss_colourmenu_selected is then a
 * single pointer compare plus the item index. */
struct wuss_colourmenu
{
  const wuss_t *wuss; /* borrowed; supplies the allocator for teardown */
  wuss_menu_t  *menu; /* owned; every block via wuss->alloc, see menu_free */
};

/* Free a menu built here: text, items, title, node -- all through wuss's
 * allocator, so wuss_menu_destroy (plain free) must not be used. Tolerates
 * NULL text for partial unwinding. */
static void menu_free(const wuss_t *wuss, wuss_menu_t *m)
{
  int i;

  if (m == NULL)
    return;

  for (i = 0; i < m->nitems; i++)
    wuss__free(wuss, (void *) m->items[i].text);
  wuss__free(wuss, (void *) m->items);
  wuss__free(wuss, (void *) m->title);
  wuss__free(wuss, m);
}

/* wuss__ has no strdup; malloc + copy. */
static char *wuss__strdup(const wuss_t *wuss, const char *s)
{
  size_t len;
  char  *copy;

  len  = strlen(s) + 1;
  copy = wuss__malloc(wuss, len);
  if (copy != NULL)
    memcpy(copy, s, len);
  return copy;
}

result_t wuss_colourmenu_create(wuss_colourmenu_t **out,
                                const wuss_t       *wuss,
                                const char         *title)
{
  wuss_colourmenu_t *cm;
  wuss_menu_t       *m;
  wuss_menu_item_t  *items;
  int                n;
  int                i;

  items = NULL;

  if (out == NULL || wuss == NULL)
    return result_NULL_ARG;

  n = wuss->npalette;

  cm = wuss__malloc(wuss, sizeof(*cm));
  if (cm == NULL)
    return result_OOM;
  cm->wuss = wuss;
  cm->menu = NULL;

  m = wuss__malloc(wuss, sizeof(*m));
  if (m == NULL)
  {
    wuss__free(wuss, cm);
    return result_OOM;
  }
  m->title  = NULL;
  m->items  = NULL;
  m->nitems = 0;

  if (n > 0)
  {
    items = wuss__malloc(wuss, (size_t) n * sizeof(*items));
    if (items == NULL)
    {
      menu_free(wuss, m);
      wuss__free(wuss, cm);
      return result_OOM;
    }
    memset(items, 0, (size_t) n * sizeof(*items));
    m->items  = items;
    m->nitems = n; /* items zeroed: menu_free's NULL-text loop is safe now */
  }

  m->title = wuss__strdup(wuss, title ? title : "Colour");
  if (m->title == NULL)
  {
    menu_free(wuss, m);
    wuss__free(wuss, cm);
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

    items[i].text = wuss__strdup(wuss, label);
    if (items[i].text == NULL)
    {
      menu_free(wuss, m);
      wuss__free(wuss, cm);
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
  const wuss_t *wuss;

  if (doomed == NULL)
    return;

  wuss = doomed->wuss;
  menu_free(wuss, doomed->menu);
  wuss__free(wuss, doomed);
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
