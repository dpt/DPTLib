/* wuss/icon/set-text.c -- replace a work-area icon's label */

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss_icon_set_text(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            const char    *text)
{
  wuss_t *w;
  char   *dup;

  w   = window->wuss;
  dup = wuss__alloc_strdup(&w->alloc, text != NULL ? text : "");
  if (dup == NULL)
    return result_OOM;

  wuss__free(w, (char *) icon->spec.text);
  icon->spec.text = dup;

  wuss__icon_invalidate(window, icon);

  return result_OK;
}
