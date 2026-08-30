/* set-text.c -- wuss - replace a work-area icon's label */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

result_t wuss_icon_set_text(wuss_icon_t *icon, const char *text)
{
  const char *src;
  char       *dup;
  size_t      len;

  src = (text != NULL) ? text : "";
  len = strlen(src);

  dup = malloc(len + 1);
  if (dup == NULL)
    return result_OOM;
  memcpy(dup, src, len + 1);

  free(icon->text);
  icon->text = dup;

  wuss__icon_invalidate(icon);

  return result_OK;
}
