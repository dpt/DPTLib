/* blank.c -- wuss test - colour-cycling client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "blank.h"

#define BLANK_CYCLE_FRAMES 30 /* colour advances every half-second at 60fps */

void blank_step(wuss_window_t *window, blank_client_t *bc)
{
  if (++bc->frame_count < BLANK_CYCLE_FRAMES)
    return;

  bc->frame_count = 0;
  bc->index       = (bc->index + 1) % bc->npalette;

  wuss_window_set_background(window, bc->index);
}

#endif /* USE_SDL */
