/* client-make.c -- wuss - minimal window manager */

#include "wuss/window.h"

wuss_client_t wuss_client_make(wuss_redraw_fn_t *redraw, wuss_mouse_fn_t *mouse, void *client_data, wuss_colour_t bg)
{
  wuss_client_t client;

  client.redraw      = redraw;
  client.mouse       = mouse;
  client.client_data = client_data;
  client.bg          = bg;

  return client;
}
