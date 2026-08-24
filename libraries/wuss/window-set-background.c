/* window-set-background.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_window_set_background(wuss_window_t *window, wuss_colour_t bg)
{
  box_t content;

  if (bg != wuss_NO_BACKGROUND && (bg < 0 || bg >= window->wuss->npalette))
    return result_WUSS_BAD_COLOUR;

  window->client.bg = bg;

  wuss__content_box(window, &content);
  wuss__invalidate_clipped(window, &content);

  return result_OK;
}
