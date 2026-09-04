/* framebuf/screen/screen-fill-hline.c -- horizontal run fill */

#include <assert.h>
#include <stddef.h>

#include "framebuf/colour.h"

#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

void screen_fill_hline(screen_t *scr, int x, int y, int w, colour_t colour)
{
  box_t          clip_box;
  box_t          run_box;
  box_t          draw_box;
  int            clipped_width;
  pixelfmt_any_t fmt;
  unsigned char *rowp;

  if (w <= 0)
    return;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  run_box.x0 = x;
  run_box.y0 = y;
  run_box.x1 = x + w;
  run_box.y1 = y + 1;
  if (box_intersection(&clip_box, &run_box, &draw_box))
    return;

  clipped_width = draw_box.x1 - draw_box.x0;

  fmt = colour_to_pixel(scr->palette,
                        (scr->format == pixelfmt_p4) ? 16 : 0,
                        colour, scr->format);

  /* the per-format run fill lives in the span table; "first" lets it address
   * an odd P4 nibble so this needn't pack. scr->span is NULL for a pixelfmt_t
   * with no span-registry entry -- the assert catches that in debug builds,
   * but a release build must still no-op rather than dereference NULL. */
  assert(scr->span && scr->span->fill);
  if (scr->span == NULL)
    return;

  rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
  scr->span->fill(rowp, draw_box.x0, fmt, clipped_width);
}
