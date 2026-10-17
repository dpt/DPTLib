/* wuss/clipboard.c -- shared text clipboard */

#include <assert.h>
#include <stddef.h>

#include "impl.h"

result_t wuss_clipboard_set(wuss_t *wuss, const char *utf8)
{
  char *copy;

  assert(wuss != NULL);

  if (utf8 == NULL)
  {
    copy = NULL;
  }
  else
  {
    copy = wuss__alloc_strdup(&wuss->alloc, utf8);
    if (copy == NULL)
      return result_OOM;
  }

  wuss__free(wuss, wuss->clipboard);
  wuss->clipboard = copy;
  wuss->clipboard_gen++;

  return result_OK;
}

const char *wuss_clipboard_get(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->clipboard != NULL ? wuss->clipboard : "";
}

unsigned int wuss_clipboard_generation(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->clipboard_gen;
}
