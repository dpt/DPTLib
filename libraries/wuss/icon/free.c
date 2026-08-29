/* free.c -- wuss - free a window's whole icon store */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

void wuss__icons_free(wuss_window_t *window)
{
  int i;

  for (i = 0; i < window->nicons; i++)
  {
    free(window->icons[i]->text);
    free(window->icons[i]);
  }

  free(window->icons);
  window->icons     = NULL;
  window->nicons    = 0;
  window->cap_icons = 0;
}
