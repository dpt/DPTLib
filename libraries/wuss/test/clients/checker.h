/* checker.h -- wuss test - checkerboard client */

#ifndef CLIENTS_CHECKER_H
#define CLIENTS_CHECKER_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* fills the whole content area with a black and white pixel checkerboard */
typedef struct checker_client
{
  colour_t black, white;
}
checker_client_t;

wuss_redraw_fn_t checker_redraw;

#endif /* USE_SDL */

#endif /* CLIENTS_CHECKER_H */
