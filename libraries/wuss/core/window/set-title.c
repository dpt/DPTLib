/* wuss/window/set-title.c -- wuss - minimal window manager */

#include <string.h>

#include "../impl.h"

void wuss_window_set_title(wuss_window_t *window, const char *title)
{
#ifdef WUSS_FURNITURE
  if (title == NULL)
    title = "";

  if (strncmp(window->title, title, WUSS_TITLE_MAX) == 0)
    return;

  wuss__writable_copy(window->title, WUSS_TITLE_MAX + 1, title);

  wuss__chrome_repaint(window);
#else
  (void) window;
  (void) title;
#endif
}
