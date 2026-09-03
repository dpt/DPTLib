/* wuss/component/fontmenu.c -- a menu of the available bitmap fonts */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "utils/array.h"

#include "wuss/menu.h"
#include "wuss/task.h"

#include "wuss/component/fontmenu.h"

/* ----------------------------------------------------------------------- */

/* ponytail: flat list, one item per font, no family/weight grouping into
 * submenus. wuss_fontmenu_selected then needs only a single pointer compare.
 * Add a grouping pass (and widen the identity check to the submenus) if a
 * font directory ever gets big enough to want it. */
struct wuss_fontmenu
{
  wuss_menu_t *menu; /* owned; freed by wuss_menu_destroy */
};

/* Growable list of strdup'd font names, filled by the enumerate callback. */
typedef struct namelist
{
  char **names;
  int    n;
  int    cap;
  int    oom; /* a strdup/grow failed; stop and unwind */
}
namelist_t;

static result_t collect_name(const char *name,
                             const char *path,
                             void       *opaque)
{
  namelist_t *nl = opaque;
  char       *copy;

  (void) path;

  if (array_grow((void **) &nl->names, sizeof(*nl->names),
                 nl->n, &nl->cap, 1, 8))
  {
    nl->oom = 1;
    return result_OOM;
  }

  copy = strdup(name);
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
    free(nl->names[i]);
  free(nl->names);
  nl->names = NULL;
  nl->n = nl->cap = 0;
}

static int name_cmp(const void *a, const void *b)
{
  return strcmp(*(const char *const *) a, *(const char *const *) b);
}

/* ----------------------------------------------------------------------- */

/* Build a flat wuss_menu_t whose items[i].text is names[i], transferring
 * ownership of each string. On failure every string is freed and *out
 * untouched. */
static result_t build_menu(char        **names,
                           int           nnames,
                           const char   *title,
                           wuss_menu_t **out)
{
  wuss_menu_t      *m;
  wuss_menu_item_t *items;
  int               i;

  m = malloc(sizeof(*m));
  if (m == NULL)
    return result_OOM;

  items = (nnames > 0) ? calloc((size_t) nnames, sizeof(*items)) : NULL;
  if (nnames > 0 && items == NULL)
  {
    free(m);
    return result_OOM;
  }

  m->title = strdup(title ? title : "Font");
  if (m->title == NULL)
  {
    free(items);
    free(m);
    return result_OOM;
  }

  for (i = 0; i < nnames; i++)
  {
    items[i].text    = names[i]; /* ownership transferred */
    items[i].flags   = wuss_MENU_ITEM_NONE;
    items[i].submenu = NULL;
    items[i].window  = NULL;
  }

  m->items  = items;
  m->nitems = nnames;

  *out = m;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t wuss_fontmenu_create(wuss_fontmenu_t **out,
                              const char       *dir,
                              const char       *title)
{
  namelist_t       nl;
  wuss_fontmenu_t *fm;
  result_t         rc;

  if (out == NULL || dir == NULL)
    return result_NULL_ARG;

  memset(&nl, 0, sizeof(nl));

  rc = bmfont_enumerate(dir, collect_name, &nl);
  if (rc != result_OK)
  {
    namelist_free(&nl);
    return nl.oom ? result_OOM : rc;
  }

  if (nl.n > 1)
    qsort(nl.names, (size_t) nl.n, sizeof(nl.names[0]), name_cmp);

  fm = malloc(sizeof(*fm));
  if (fm == NULL)
  {
    namelist_free(&nl);
    return result_OOM;
  }

  rc = build_menu(nl.names, nl.n, title, &fm->menu);
  if (rc != result_OK)
  {
    namelist_free(&nl); /* build_menu took nothing on failure */
    free(fm);
    return rc;
  }

  /* build_menu adopted every string; drop the array, keep the pointers. */
  free(nl.names);

  *out = fm;
  return result_OK;
}

void wuss_fontmenu_destroy(wuss_fontmenu_t *doomed)
{
  if (doomed == NULL)
    return;

  wuss_menu_destroy(doomed->menu);
  free(doomed);
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
