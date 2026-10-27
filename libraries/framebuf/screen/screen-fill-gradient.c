/* framebuf/screen/screen-fill-gradient.c -- fill a box with a linear gradient */

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

void screen_fill_linear_gradient(screen_t    *scr,
                                 const box_t *box,
                                 point_t      a,
                                 point_t      b,
                                 colour_t     c0,
                                 colour_t     c1)
{
  double         dx, dy, len2;
  box_t          clip_box;
  box_t          draw_box;
  pixelfmt_any_t lut[256];
  int            i;
  int            x, y;
  int            x0, step;

  dx   = b.x - a.x;
  dy   = b.y - a.y;
  len2 = dx * dx + dy * dy;
  if (len2 == 0)
    return;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (box_intersection(box, &clip_box, &draw_box))
    return; /* nothing visible */

  for (i = 0; i < 256; i++)
    lut[i] = screen_colour_to_pixel(scr, colour_mix_rgba(c0, c1, i, 255));

  /* one rect fill per run of pixels taking the same step */
  for (y = draw_box.y0; y < draw_box.y1; y++)
  {
    x0   = draw_box.x0;
    step = -1;
    for (x = draw_box.x0; x <= draw_box.x1; x++)
    {
      if (x < draw_box.x1)
      {
        i = CLAMP((int) (((x - a.x) * dx + (y - a.y) * dy) / len2 * 255 + 0.5),
                  0, 255);
        if (i == step)
          continue;
      }

      if (step >= 0)
        screen_fill_rect_value(scr, x0, y, SIZE2D(x - x0, 1), lut[step]);

      x0   = x;
      step = i;
    }
  }
}

/* vim: set ts=8 sts=2 sw=2 et: */
