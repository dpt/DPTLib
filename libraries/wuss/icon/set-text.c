/* wuss/icon/set-text.c -- replace a work-area icon's label */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss_icon_set_text(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            const char    *text)
{
  wuss_t     *w;
  const char *src;
  char       *dup;
  size_t      len;

  w   = window->wuss;
  src = (text != NULL) ? text : "";
  len = strlen(src);

  dup = wuss__malloc(w, len + 1);
  if (dup == NULL)
    return result_OOM;
  memcpy(dup, src, len + 1);

  wuss__free(w, (char *) icon->spec.text);
  icon->spec.text = dup;

  wuss__icon_invalidate(window, icon);

  return result_OK;
}
