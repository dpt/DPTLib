/* blank.c -- wuss test - colour-cycling client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"

#include "blank.h"

#define BLANK_CYCLE_FRAMES 30 /* colour advances every half-second at 60fps */

void blank_step(wuss_window_t *window, blank_client_t *bc)
{
  result_t rc;

  if (++bc->frame_count < BLANK_CYCLE_FRAMES)
    return;

  bc->frame_count = 0;
  bc->index       = (bc->index + 1) % bc->npalette;

  rc = wuss_window_set_background(window, bc->index);
  if (rc != result_OK)
    logf_warning("blank_step: wuss_window_set_background(%d) failed", bc->index);
}

#endif /* USE_SDL */
