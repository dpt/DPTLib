/* wuss/window/set-width-quantum.c -- wuss - minimal window manager */

#include "../impl.h"

result_t wuss_window_set_width_quantum(wuss_window_t *window,
                                       int            step,
                                       int            base)
{
  if (step < 0 || base < 0)
    return result_BAD_ARG;

  window->width_step = step;
  window->width_base = base;

  return result_OK;
}
