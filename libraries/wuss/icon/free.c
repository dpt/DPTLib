/* free.c -- wuss - free a window's whole icon store */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

void wuss__icons_free(wuss_window_t *window)
{
  wuss_t *w;
  int     i;

  w = window->wuss;

  for (i = 0; i < window->nicons; i++)
  {
    wuss__free(w, window->icons[i]->text);
    wuss__free(w, window->icons[i]);
  }

  wuss__free(w, window->icons);
  window->icons     = NULL;
  window->nicons    = 0;
  window->cap_icons = 0;
}
