/* wuss/window/set-min-size.c -- wuss - minimal window manager */

#include "../impl.h"

result_t wuss_window_set_min_size(wuss_window_t *window, size2d_t min)
{
  if (min.w < wuss_KEEP || min.h < wuss_KEEP)
    return result_BAD_ARG;

  if (min.w != wuss_KEEP)
    window->min_doc.w = min.w;
  if (min.h != wuss_KEEP)
    window->min_doc.h = min.h;

  return result_OK;
}
