/* wuss/window/toggle-size.c -- toggle a window between its two sizes */

#include <stddef.h>

#include "wuss/window.h"

#include "../impl.h"

void wuss_window_toggle_size(wuss_window_t *window)
{
#ifdef WUSS_FURNITURE
  if (window == NULL || !(window->flags & wuss_WINDOW_TOGGLE_SIZE))
    return;

  window->wuss->furniture_ops->toggle_size(window);
#else
  /* no furniture, so no toggle-size icon to mimic */
  (void) window;
#endif
}
