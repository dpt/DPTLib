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
 * font directory ever gets big enough to want it. */
struct wuss_fontmenu
{
  wuss_alloc_t alloc; /* copied hooks; every block below goes through these */
  wuss_menu_t *menu;  /* owned; freed by menu_free, not wuss_menu_destroy */
};

/* Free a menu built here -- text, items, title, node -- through the same
 * hooks it was built with. Tolerates NULL text for partial unwinding. */
static void menu_free(const wuss_alloc_t *a, wuss_menu_t *m)
{
  int i;

  if (m == NULL)
    return;

  for (i = 0; i < m->nitems; i++)
    a->free((void *) m->items[i].text); /* discard const */
  a->free((void *) m->items);           /* discard const */
  a->free((void *) m->title);           /* discard const */
  a->free(m);
}

/* ----------------------------------------------------------------------- */

/* Growable list of name copies, filled by the enumerate callback. Transient:
 * freed before create returns. Routed through the caller's hooks so even the
 * scratch never touches raw stdlib. */
typedef struct namelist
{
  const wuss_alloc_t *alloc; /* borrowed; the resolved hooks */
  char              **names;
  int                 n;
  int                 cap;
  int                 oom; /* a strdup/grow failed; stop and unwind */
}
namelist_t;

static result_t collect_name(const char *name,
                             const char *path,
                             void       *opaque)
{
  namelist_t *nl = opaque;
  char       *copy;

  (void) path;

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

/* Build a flat wuss_menu_t through \p a, copying each names[i] in (the
 * scratch list keeps its own copies). On failure everything taken is freed
 * through \p a and *out is untouched. */
static result_t build_menu(const wuss_alloc_t *a,
                           char              **names,
                           int                 nnames,
                           const char         *title,
                           wuss_menu_t       **out)
{
  wuss_menu_t      *m;
  wuss_menu_item_t *items;
  int               i;

  items = NULL;

  m = a->malloc(sizeof(*m));
  if (m == NULL)
    return result_OOM;
  m->title  = NULL;
  m->items  = NULL;
  m->nitems = 0;

  if (nnames > 0)
  {
    items = a->malloc((size_t) nnames * sizeof(*items));
    if (items == NULL)
    {
      menu_free(a, m);
      return result_OOM;
    }
    memset(items, 0, (size_t) nnames * sizeof(*items));
    m->items  = items;
    m->nitems = nnames; /* items zeroed: menu_free's NULL-text loop is safe */
  }

  m->title = wuss__alloc_strdup(a, title ? title : "Font");
  if (m->title == NULL)
  {
    menu_free(a, m);
    return result_OOM;
  }

  for (i = 0; i < nnames; i++)
  {
    items[i].text = wuss__alloc_strdup(a, names[i]);
    if (items[i].text == NULL)
    {
      menu_free(a, m);
      return result_OOM;
    }
    items[i].flags   = wuss_MENU_ITEM_NONE;
    items[i].submenu = NULL;
    items[i].window  = NULL;
  }

  *out = m;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t wuss_fontmenu_create(wuss_fontmenu_t   **out,
                              const char         *dir,
                              const char         *title,
                              const wuss_alloc_t *alloc)
{
  namelist_t       nl;
  wuss_fontmenu_t *fm;
  result_t         rc;

  if (out == NULL || dir == NULL)
    return result_NULL_ARG;

  if (alloc == NULL)
    alloc = &wuss_alloc;

  memset(&nl, 0, sizeof(nl));
  nl.alloc = alloc;

  rc = bmfont_enumerate(dir, collect_name, &nl);
  if (rc != result_OK)
  {
    namelist_free(&nl);
    return nl.oom ? result_OOM : rc;
  }

  if (nl.n > 1)
    qsort(nl.names, (size_t) nl.n, sizeof(nl.names[0]), name_cmp);

  fm = alloc->malloc(sizeof(*fm));
  if (fm == NULL)
  {
    namelist_free(&nl);
    return result_OOM;
  }
  fm->alloc = *alloc;
  fm->menu  = NULL;

  rc = build_menu(&fm->alloc, nl.names, nl.n, title, &fm->menu);
  namelist_free(&nl); /* build_menu copied what it needed */
  if (rc != result_OK)
  {
    alloc->free(fm);
    return rc;
  }

  *out = fm;
  return result_OK;
}

void wuss_fontmenu_destroy(wuss_fontmenu_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  menu_free(&alloc, doomed->menu);
  alloc.free(doomed);
}

const wuss_menu_t *wuss_fontmenu_menu(const wuss_fontmenu_t *fm)
{
  return fm ? fm->menu : NULL;
}

const char *wuss_fontmenu_selected(const wuss_fontmenu_t *fm,
                                   const wuss_event_t    *ev)
{
  int index;

  if (fm == NULL || ev == NULL)
    return NULL;
  if (ev->kind != wuss_EVENT_MENU_SELECT)
    return NULL;
  if (ev->data.menu_select.menu != fm->menu)
    return NULL;

  index = ev->data.menu_select.index;
  if (index < 0 || index >= fm->menu->nitems)
    return NULL;

  return fm->menu->items[index].text;
}

void wuss_fontmenu_set_ticked(wuss_fontmenu_t *fm, int index)
{
  wuss_menu_item_t *items;
  int               i;

  if (fm == NULL)
    return;

  /* fm->menu is ours to mutate; only wuss_fontmenu_menu hands it out const */
  items = (wuss_menu_item_t *) fm->menu->items;

  for (i = 0; i < fm->menu->nitems; i++)
    if (i == index)
      items[i].flags |= wuss_MENU_ITEM_TICKED;
    else
      items[i].flags &= ~(wuss_menu_item_flags_t) wuss_MENU_ITEM_TICKED;
}
