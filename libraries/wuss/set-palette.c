/* wuss/set-palette.c -- swap the system palette mid-session */

#include <assert.h>
#include <string.h>

#include "geom/box.h"

#include "impl.h"

result_t wuss_set_palette(wuss_t         *wuss,
                          const colour_t *palette,
                          int             npalette)
{
  wuss_event_t event;
  box_t        screen;
  result_t     rc;
  list_t      *e;

  assert(wuss    != NULL);
  assert(palette != NULL);

  /* Same length only: keeps every stored furniture/bevel/backdrop colour
   * index in range, and matches the fixed-size framebuffer palette on a
   * paletted screen. */
  if (npalette != wuss->npalette)
    return result_BAD_ARG;

  memcpy(wuss->palette, palette, npalette * sizeof(*wuss->palette));

  wuss->palettecache.white = wuss_nearest_colour(wuss, 255, 255, 255);
  wuss->palettecache.black = wuss_nearest_colour(wuss, 0, 0, 0);

  rc = result_OK;
  event.kind = wuss_EVENT_PALETTE;
  for (e = wuss->tasks.next; e != NULL; e = e->next)
  {
    wuss_task_t *task;
    result_t     crc;

    task = (wuss_task_t *) e;

    crc = wuss__deliver(task, NULL, &event);
    if (crc != result_OK && rc == result_OK)
      rc = crc;
  }

  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = wuss->scr->size.w;
  screen.y1 = wuss->scr->size.h;
  wuss_invalidate(wuss, &screen);

  return rc;
}
