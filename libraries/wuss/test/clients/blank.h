/* blank.h -- wuss test - colour-cycling client */

#ifndef CLIENTS_BLANK_H
#define CLIENTS_BLANK_H

#ifdef USE_SDL

#include "wuss/window.h"
#include "wuss/wuss.h"

/* window C's client: no redraw callback at all, relying entirely on wuss's
 * managed background fill; blank_step periodically hands wuss a new
 * palette index so the fill colour cycles over time */
typedef struct blank_client
{
  int npalette;
  int index;
  int frame_count;
}
blank_client_t;

/* advance the cycle and, every few frames, push the next palette index as
 * the window's background; called once per frame from the main loop */
void blank_step(wuss_window_t *window, blank_client_t *bc);

#endif /* USE_SDL */

#endif /* CLIENTS_BLANK_H */
