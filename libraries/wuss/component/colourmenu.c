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

#include "../impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list, one item per palette entry, index order. No grouping
 * of near colours, no "recent" section. wuss_colourmenu_selected is then a
 * single pointer compare plus the item index. */
struct wuss_colourmenu
{
  wuss_alloc_t alloc; /* copied hooks; the wuss_t itself is not retained */
  wuss_menu_t *menu;  /* owned; every block via alloc, see menu_free */
};

/* wuss_alloc_t has no strdup; malloc + copy. */
static char *alloc_strdup(const wuss_alloc_t *a, const char *s)
{
  size_t len;
  char  *copy;

  len  = strlen(s) + 1;
  copy = a->malloc(len);
  if (copy != NULL)
    memcpy(copy, s, len);
  return copy;
}

/* Free a menu built here -- text, items, title, node -- through the same
 * hooks it was built with, so wuss_menu_destroy (plain free) must not be
 * used. Tolerates NULL text for partial unwinding. */
static void menu_free(const wuss_alloc_t *a, wuss_menu_t *m)
{
  int i;

  if (m == NULL)
    return;

  for (i = 0; i < m->nitems; i++)
    a->free((void *) m->items[i].text);
  a->free((void *) m->items);
  a->free((void *) m->title);
  a->free(m);
}

result_t wuss_colourmenu_create(wuss_colourmenu_t **out,
                                const wuss_t       *wuss,
                                const char         *title)
{
  const wuss_alloc_t *a;
  wuss_colourmenu_t  *cm;
  wuss_menu_t        *m;
  wuss_menu_item_t   *items;
  int                 n;
  int                 i;

  items = NULL;

  if (out == NULL || wuss == NULL)
    return result_NULL_ARG;

  a = &wuss->alloc;
  n = wuss->npalette;

  cm = a->malloc(sizeof(*cm));
  if (cm == NULL)
    return result_OOM;
  cm->alloc = *a;
  cm->menu  = NULL;
  a = &cm->alloc; /* use the copy from here on -- outlives the wuss_t */

  m = a->malloc(sizeof(*m));
  if (m == NULL)
  {
    a->free(cm);
    return result_OOM;
  }
  m->title  = NULL;
  m->items  = NULL;
  m->nitems = 0;

  if (n > 0)
  {
    items = a->malloc((size_t) n * sizeof(*items));
    if (items == NULL)
    {
      menu_free(a, m);
      a->free(cm);
      return result_OOM;
    }
    memset(items, 0, (size_t) n * sizeof(*items));
    m->items  = items;
    m->nitems = n; /* items zeroed: menu_free's NULL-text loop is safe now */
  }

  m->title = alloc_strdup(a, title ? title : "Colour");
  if (m->title == NULL)
  {
    menu_free(a, m);
    a->free(cm);
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

    items[i].text = alloc_strdup(a, label);
    if (items[i].text == NULL)
    {
      menu_free(a, m);
      a->free(cm);
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
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  menu_free(&alloc, doomed->menu);
  alloc.free(doomed);
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
