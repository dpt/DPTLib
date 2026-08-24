/* palette.h -- wuss test - desktop palette swatch grid client */

#ifndef CLIENTS_PALETTE_H
#define CLIENTS_PALETTE_H

#ifdef USE_SDL

#include "framebuf/colour.h"
#include "wuss/window.h"

/* window D's client: draws every entry of the desktop palette as a square
 * in a grid, so the palette is visible at a glance */
typedef struct palette_client
{
  const colour_t *palette;
  int             npalette;
}
palette_client_t;

wuss_redraw_fn_t palette_redraw;

#endif /* USE_SDL */

#endif /* CLIENTS_PALETTE_H */
