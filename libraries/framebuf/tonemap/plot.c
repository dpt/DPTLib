/* framebuf/tonemap/plot.c -- draw a tone map's curves */

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "geom/box.h"

#include "framebuf/tonemap.h"

/* ----------------------------------------------------------------------- */

void tonemap_plot(tonemap_t         *map,
                  tonemap_channels_t channels,
                  screen_t          *scr,
                  const box_t       *area,
                  int                stroke)
{
  static const struct
  {
    tonemap_channels_t channel;
    unsigned int       r, g, b;
  }
  curves[] =
  {
    { tonemap_CHANNEL_ALPHA, 128, 128, 128 },
    { tonemap_CHANNEL_RED,   224,   0,   0 },
    { tonemap_CHANNEL_GREEN,   0, 160,   0 },
    { tonemap_CHANNEL_BLUE,    0,   0, 224 },
  };

  const tonemap_table_t *tables[4];
  colour_t               grid;
  int                    w, h;
  int                    c;
  int                    i;

  w = area->x1 - area->x0;
  h = area->y1 - area->y0;
  if (w < 2 || h < 2)
    return;

  screen_fill_rect(scr, area->x0, area->y0, SIZE2D(w, h),
                   colour_rgb(255, 255, 255));

  /* quarter-way grid lines */
  grid = colour_rgb(208, 208, 208);
  for (i = 1; i < 4; i++)
  {
    screen_draw_line(scr, area->x0 + (w - 1) * i / 4, area->y0,
                     area->x0 + (w - 1) * i / 4, area->y1 - 1, grid);
    screen_draw_line(scr, area->x0, area->y0 + (h - 1) * i / 4,
                     area->x1 - 1, area->y0 + (h - 1) * i / 4, grid);
  }

  tonemap_get_tables(map, &tables[1], &tables[2], &tables[3], &tables[0]);

  for (c = 0; c < NELEMS(curves); c++)
  {
    colour_t colour;

    if (!(channels & curves[c].channel))
      continue;

    colour = colour_rgb(curves[c].r, curves[c].g, curves[c].b);
    for (i = 1; i < 256; i++)
    {
      /* y is flipped: a level of 255 plots at the top */
      screen_draw_thick_line(
        scr,
        area->x0 + (i - 1) * (w - 1) / 255,
        area->y1 - 1 - tables[c]->level[i - 1] * (h - 1) / 255,
        area->x0 + i * (w - 1) / 255,
        area->y1 - 1 - tables[c]->level[i] * (h - 1) / 255,
        stroke,
        colour);
    }
  }
}
