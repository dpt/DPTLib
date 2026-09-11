/* wuss/icon/create.c -- create a work-area icon */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss_icon_create(wuss_window_t          *window,
                          const wuss_icon_spec_t *spec,
                          wuss_icon_t           **icon)
{
  result_t     rc;
  wuss_t      *w;
  wuss_icon_t *it;
  wuss_icon_t  scratch;

  assert(window != NULL);
  assert(spec   != NULL);

  w = window->wuss;

  rc = wuss__icon_from_spec(w, spec, &scratch);
  if (rc != result_OK)
    return rc;

  if (wuss__array_grow(&w->alloc, (void **) &window->icons,
                       sizeof(*window->icons), window->nicons,
                       &window->cap_icons, 1, 4) != 0)
    return result_OOM;

  it = wuss__malloc(w, sizeof(*it));
  if (it == NULL)
    return result_OOM;

  *it = scratch; /* scratch.spec.text aliases spec->text; replaced with an owned copy below */

  it->spec.text = wuss__alloc_strdup(&w->alloc,
                                     spec->text != NULL ? spec->text : "");
  if (it->spec.text == NULL)
  {
    wuss__free(w, it);
    return result_OOM;
  }

  window->icons[window->nicons++] = it;

  wuss__icon_invalidate(window, it);

  if (icon != NULL)
    *icon = it;

  return result_OK;
}
