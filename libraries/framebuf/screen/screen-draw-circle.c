/* framebuf/screen/screen-draw-circle.c -- circle outline and fill */

#include <stdlib.h>

#include "base/utils.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* One octant of a midpoint circle, mirrored to the other seven. screen_set_pixel
 * clips each plot, so nothing here needs its own bounds check. */
void screen_draw_circle(screen_t *scr,
                        int       cx,
                        int       cy,
                        int       r,
                        colour_t  colour)
{
  int x, y, err;

  if (r < 0)
    return;

  if (r == 0)
  {
    screen_set_pixel(scr, cx, cy, colour);
    return;
  }

  x   = r;
  y   = 0;
  err = 1 - r;

  while (x >= y)
  {
    screen_set_pixel(scr, cx + x, cy + y, colour);
    screen_set_pixel(scr, cx - x, cy + y, colour);
    screen_set_pixel(scr, cx + x, cy - y, colour);
    screen_set_pixel(scr, cx - x, cy - y, colour);
    screen_set_pixel(scr, cx + y, cy + x, colour);
    screen_set_pixel(scr, cx - y, cy + x, colour);
    screen_set_pixel(scr, cx + y, cy - x, colour);
    screen_set_pixel(scr, cx - y, cy - x, colour);

    y++;
    if (err < 0)
    {
      err += 2 * y + 1;
    }
    else
    {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

/* Same octant stepping as the outline, but each step emits a pair of solid
 * horizontal runs (via screen_fill_hline, which clips) rather than eight
 * points. Runs from the two octant families cover every scanline of the
 * disc exactly once. */
void screen_fill_circle(screen_t *scr,
                        int       cx,
                        int       cy,
                        int       r,
                        colour_t  colour)
{
  int x, y, err;

  if (r < 0)
    return;

  if (r == 0)
  {
    screen_set_pixel(scr, cx, cy, colour);
    return;
  }

  x   = r;
  y   = 0;
  err = 1 - r;

  while (x >= y)
  {
    /* the wide pair: rows cy +/- y, spanning -x..+x */
    screen_fill_hline(scr, cx - x, cy + y, 2 * x + 1, colour);
    if (y != 0)
      screen_fill_hline(scr, cx - x, cy - y, 2 * x + 1, colour);

    /* the tall pair: rows cy +/- x, spanning -y..+y; skip while it would
     * fall inside the wide pair's rows to avoid overdraw */
    if (x != y)
    {
      screen_fill_hline(scr, cx - y, cy + x, 2 * y + 1, colour);
      screen_fill_hline(scr, cx - y, cy - x, 2 * y + 1, colour);
    }

    y++;
    if (err < 0)
    {
      err += 2 * y + 1;
    }
    else
    {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}
