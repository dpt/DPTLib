/* wuss/component/fontmenu.c -- a menu of the available bitmap fonts */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"

#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

#include "wuss/component/fontmenu.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list, one item per font, no family/weight grouping into
 * submenus. wuss_fontmenu_selected then needs only a single pointer compare.
 * Add a grouping pass (and widen the identity check to the submenus) if a
 * font directory ever gets big enough to want it. One process-wide
 * singleton -- no per-task instances, no create/destroy -- rebuilt lazily
 * when the caller's dir or wuss_t changes. */
static struct
{
  wuss_alloc_t  alloc;
  char         *dir;   /* owned copy; whose fonts the menu was built from --
                        * callers pass a pathf() scratch pointer, so this
                        * cannot just borrow it */
  const wuss_t *wuss;  /* borrowed; consulted to skip SYSTEM fonts */
  wuss_menu_t  *menu;
}
g;

/* Free the singleton's owned blocks -- text, items, title, node, dir --
 * through the same hooks it was built with, leaving g.menu/g.dir/g.wuss
 * cleared so the next call rebuilds from scratch. Tolerates NULL text for
 * partial unwinding. */
static void fontmenu_free(void)
{
  int i;

  if (g.menu == NULL)
    return; /* never built, or already freed -- g.alloc may be unset */

  for (i = 0; i < g.menu->nitems; i++)
    g.alloc.free((void *) g.menu->items[i].text); /* discard const */
  g.alloc.free((void *) g.menu->items);           /* discard const */
  g.alloc.free((void *) g.menu->title);           /* discard const */
  g.alloc.free(g.menu);
  g.alloc.free(g.dir);
  g.menu = NULL;
  g.dir  = NULL;
  g.wuss = NULL;
}

/* ----------------------------------------------------------------------- */

/* Growable list of name copies, filled by the enumerate callback. Transient:
 * freed before create returns. Routed through the caller's hooks so even the
 * scratch never touches raw stdlib. */
typedef struct namelist
{
  const wuss_alloc_t *alloc; /* borrowed; the resolved hooks */
  const wuss_t       *wuss;  /* borrowed; consulted to skip SYSTEM fonts, or
                              * NULL to skip nothing */
  char              **names;
  int                 n;
  int                 cap;
  int                 oom; /* a strdup/grow failed; stop and unwind */
}
namelist_t;

/* True if any wuss_create font slot names \p name and is classed SYSTEM --
 * chrome-only, not meant to be offered as a text font. */
static int is_system_font_name(const wuss_t *wuss, const char *name)
{
  int i;

  if (wuss == NULL)
    return 0;

  for (i = 0; i < wuss_MAX_FONTS; i++)
  {
    const char *slot_name = wuss_get_font_name_n(wuss, i);

    if (slot_name != NULL && strcmp(slot_name, name) == 0 &&
        wuss_get_font_class_n(wuss, i) == wuss_FONT_CLASS_SYSTEM)
      return 1;
  }

  return 0;
}

static result_t collect_name(const char *name,
                             const char *path,
                             void       *opaque)
{
  namelist_t *nl = opaque;
  char       *copy;

  (void) path;

  if (is_system_font_name(nl->wuss, name))
    return result_OK;

  if (wuss__array_grow(nl->alloc, (void **) &nl->names, sizeof(*nl->names),
                       nl->n, &nl->cap, 1, 8))
  {
    nl->oom = 1;
    return result_OOM;
  }

  copy = wuss__alloc_strdup(nl->alloc, name);
  if (copy == NULL)
  {
    nl->oom = 1;
    return result_OOM;
  }

  nl->names[nl->n++] = copy;
  return result_OK;
}

static void namelist_free(namelist_t *nl)
{
  int i;

  for (i = 0; i < nl->n; i++)
    nl->alloc->free(nl->names[i]);
  nl->alloc->free(nl->names);
  nl->names = NULL;
  nl->n = nl->cap = 0;
}

static int name_cmp(const void *a, const void *b)
{
  return strcmp(*(const char *const *) a, *(const char *const *) b);
}

/* ----------------------------------------------------------------------- */

/* Build the singleton's g.menu, copying each names[i] in (the scratch list
 * keeps its own copies). On failure everything taken is freed and g.menu is
 * left NULL. */
static result_t build_menu(char **names, int nnames, const char *title)
{
  wuss_menu_item_t *items;
  int               i;

  items = NULL;

  g.menu = g.alloc.malloc(sizeof(*g.menu));
  if (g.menu == NULL)
    return result_OOM;
  g.menu->title  = NULL;
  g.menu->items  = NULL;
  g.menu->nitems = 0;

  if (nnames > 0)
  {
    items = g.alloc.malloc((size_t) nnames * sizeof(*items));
    if (items == NULL)
    {
      fontmenu_free();
      return result_OOM;
    }
    memset(items, 0, (size_t) nnames * sizeof(*items));
    g.menu->items  = items;
    g.menu->nitems = nnames; /* items zeroed: fontmenu_free's NULL-text loop
                              * is safe */
  }

  g.menu->title = wuss__alloc_strdup(&g.alloc, title ? title : "Font");
  if (g.menu->title == NULL)
  {
    fontmenu_free();
    return result_OOM;
  }

  for (i = 0; i < nnames; i++)
  {
    items[i].text = wuss__alloc_strdup(&g.alloc, names[i]);
    if (items[i].text == NULL)
    {
      fontmenu_free();
      return result_OOM;
    }
    items[i].flags   = wuss_MENU_ITEM_NONE;
    items[i].submenu = NULL;
    items[i].window  = NULL;
  }

  return result_OK;
}

/* (Re)build the singleton from dir's fonts, less any wuss's SYSTEM-class
 * ones. */
static result_t fontmenu_build(const char   *dir,
                               const char   *title,
                               const wuss_t *wuss)
{
  result_t     rc;
  wuss_alloc_t alloc;
  namelist_t   nl;

  fontmenu_free();

  alloc = wuss ? wuss->alloc : wuss_alloc;

  memset(&nl, 0, sizeof(nl));
  nl.alloc = &alloc;
  nl.wuss  = wuss;

  rc = bmfont_enumerate(dir, collect_name, &nl);
  if (rc != result_OK)
  {
    namelist_free(&nl);
    return nl.oom ? result_OOM : rc;
  }

  if (nl.n > 1)
    qsort(nl.names, (size_t) nl.n, sizeof(nl.names[0]), name_cmp);

  g.alloc = alloc;

  rc = build_menu(nl.names, nl.n, title);
  namelist_free(&nl); /* build_menu copied what it needed */
  if (rc != result_OK)
    return rc;

  g.dir = wuss__alloc_strdup(&g.alloc, dir);
  if (g.dir == NULL)
  {
    fontmenu_free();
    return result_OOM;
  }
  g.wuss = wuss;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

const wuss_menu_t *wuss_fontmenu_menu(const char   *dir,
                                      const char   *title,
                                      const wuss_t *wuss)
{
  if (dir == NULL)
    return NULL;

  if (g.menu == NULL || g.dir == NULL || strcmp(dir, g.dir) != 0 ||
      wuss != g.wuss)
    if (fontmenu_build(dir, title, wuss) != result_OK)
      return NULL;

  return g.menu;
}

const char *wuss_fontmenu_selected(const wuss_event_t *ev)
{
  int index;

  if (ev == NULL || g.menu == NULL)
    return NULL;
  if (ev->kind != wuss_EVENT_MENU_SELECT)
    return NULL;
  if (ev->data.menu_select.menu != g.menu)
    return NULL;

  index = ev->data.menu_select.index;
  if (index < 0 || index >= g.menu->nitems)
    return NULL;

  return g.menu->items[index].text;
}

void wuss_fontmenu_set_ticked(int index)
{
  wuss_menu_item_t *items;
  int               i;

  if (g.menu == NULL)
    return;

  /* g.menu is ours to mutate; only wuss_fontmenu_menu hands it out const */
  items = (wuss_menu_item_t *) g.menu->items;

  for (i = 0; i < g.menu->nitems; i++)
    if (i == index)
      items[i].flags |= wuss_MENU_ITEM_TICKED;
    else
      items[i].flags &= ~(wuss_menu_item_flags_t) wuss_MENU_ITEM_TICKED;
}
