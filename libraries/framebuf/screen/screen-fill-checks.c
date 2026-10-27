/* framebuf/screen/screen-fill-checks.c -- fill a box with a checkerboard */

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

/* round towards negative infinity, so squares left of or above the origin
 * index correctly */
static int floor_div(int a, int b)
{
  return (a >= 0) ? a / b : -((-a + b - 1) / b);
}

void screen_fill_checks(screen_t    *scr,
                        const box_t *box,
                        point_t      origin,
                        int          size,
                        colour_t     even,
                        colour_t     odd)
{
  box_t          clip_box;
  box_t          draw_box;
  pixelfmt_any_t pxl[2];
  int            col0, row0, col1, row1;
  int            row, col;
  box_t          square, visible;

  if (size <= 0)
    return;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (box_intersection(box, &clip_box, &draw_box))
    return; /* nothing visible */

  /* resolve both colours once, not once per square */
  pxl[0] = screen_colour_to_pixel(scr, even);
  pxl[1] = screen_colour_to_pixel(scr, odd);

  col0 = floor_div(draw_box.x0 - origin.x, size);
  row0 = floor_div(draw_box.y0 - origin.y, size);
  col1 = floor_div(draw_box.x1 - origin.x + size - 1, size);
  row1 = floor_div(draw_box.y1 - origin.y + size - 1, size);

  /* one rect fill per square, trimmed to the draw box */
  for (row = row0; row < row1; row++)
    for (col = col0; col < col1; col++)
    {
      square.x0 = origin.x + col * size;
      square.y0 = origin.y + row * size;
      square.x1 = square.x0 + size;
      square.y1 = square.y0 + size;
      if (box_intersection(&square, &draw_box, &visible))
        continue;

      screen_fill_rect_value(scr, visible.x0, visible.y0, box_size(&visible),
                             pxl[(row + col) & 1]);
    }
}

/* vim: set ts=8 sts=2 sw=2 et: */
