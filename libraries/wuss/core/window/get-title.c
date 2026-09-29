/* wuss/window/get-title.c -- wuss - minimal window manager */

#include "../impl.h"

const char *wuss_window_get_title(const wuss_window_t *window)
{
#ifdef WUSS_FURNITURE
  return window->title;
#else
  (void) window;
  return "";
#endif
}
