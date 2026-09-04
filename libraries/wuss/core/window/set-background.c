/* wuss/window/set-background.c -- wuss - minimal window manager */

#include "../impl.h"

result_t wuss_window_set_background(wuss_window_t  *window,
                                    wuss_backdrop_t bg)
{
  box_t content;

  bg.colour     = wuss__resolve_colour(window->wuss, bg.colour);
  bg.pattern_bg = wuss__resolve_colour(window->wuss, bg.pattern_bg);

  if (wuss__validate_backdrop(window->wuss, &bg) != result_OK)
    return result_WUSS_BAD_COLOUR;

  window->bg = bg;

  wuss__content_box(window, &content);
  wuss__invalidate_clipped(window, &content);

  return result_OK;
}
