/* wuss/icon/create.c -- create a work-area icon */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

result_t wuss_icon_create(wuss_window_t          *window,
                          const wuss_icon_spec_t *spec,
                          wuss_icon_t           **icon)
{
  wuss_t       *w;
  wuss_icon_t  *it;
  wuss_icon_t **grown;
  wuss_icon_t   scratch;
  const char   *src;
  size_t        len;
  int           newcap;
  result_t      rc;

  assert(window != NULL);
  assert(spec   != NULL);

  w = window->wuss;

  rc = wuss__icon_from_spec(w, spec, &scratch);
  if (rc != result_OK)
    return rc;

  if (window->nicons == window->cap_icons)
  {
    newcap = (window->cap_icons == 0) ? 4 : window->cap_icons * 2;
    grown  = wuss__realloc(w, window->icons, newcap * sizeof(*window->icons));
    if (grown == NULL)
      return result_OOM;
    window->icons     = grown;
    window->cap_icons = newcap;
  }

  it = wuss__malloc(w, sizeof(*it));
  if (it == NULL)
    return result_OOM;

  *it = scratch; /* scratch.text aliases spec->text; replaced with an owned copy below */
  it->window = window;

  src = (spec->text != NULL) ? spec->text : "";
  len = strlen(src);
  it->text = wuss__malloc(w, len + 1);
  if (it->text == NULL)
  {
    wuss__free(w, it);
    return result_OOM;
  }
  memcpy(it->text, src, len + 1);

  window->icons[window->nicons++] = it;

  wuss__icon_invalidate(it);

  if (icon != NULL)
    *icon = it;

  return result_OK;
}
