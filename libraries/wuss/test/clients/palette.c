/* palette.c -- wuss test - desktop palette swatch grid client */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "palette.h"

result_t palette_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  palette_client_t *pc;
  int               cols, rows;
  int               cell_w, cell_h;
  int               i;

  NOT_USED(window);

  pc = client_data;

  if (pc->npalette <= 0)
    return result_OK;

  cols = 1;
  while (cols * cols < pc->npalette)
    cols++;
  rows = (pc->npalette + cols - 1) / cols;

  cell_w = (content->x1 - content->x0) / cols;
  cell_h = (content->y1 - content->y0) / rows;

  for (i = 0; i < pc->npalette; i++)
  {
    int col, row, x, y;

    col = i % cols;
    row = i / cols;
    x   = content->x0 + col * cell_w;
    y   = content->y0 + row * cell_h;

    screen_draw_rect(scr, x, y, cell_w, cell_h, pc->palette[i]);
  }

  return result_OK;
}

#endif /* USE_SDL */
