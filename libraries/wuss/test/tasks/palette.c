/* palette.c -- wuss test - desktop palette swatch grid task */

#ifdef USE_SDL

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"

#include "palette.h"

result_t palette_create(wuss_t         *wuss,
                        const colour_t *palette,
                        int             npalette,
                        palette_task_t *task)
{
  wuss_task_t delegate;
  box_t       box;

  task->palette  = palette;
  task->npalette = npalette;

  delegate = wuss_task_start(palette_handle, task, palette_PICO8_BLACK); /* backdrop for any rounding gap around the grid */
  box      = (box_t) BOX_POS_SIZE(380, 260, 100, 100);

  return wuss_window_create(wuss, &box, "Palette", wuss_WINDOW_NONE, &delegate, &task->window);
}

void palette_destroy(palette_task_t *task)
{
  wuss_window_destroy(task->window);
}

static result_t palette_redraw(wuss_window_t *window,
                               screen_t      *scr,
                               const box_t   *content,
                               void          *task_data)
{
  palette_task_t *pc;
  int               cols, rows;
  int               cell_w, cell_h;
  int               i;

  NOT_USED(window);

  pc = task_data;

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

result_t palette_handle(wuss_window_t     *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  if (event->kind != wuss_EVENT_REDRAW)
    return result_OK;

  return palette_redraw(window, event->data.redraw.scr, event->data.redraw.content, task_data);
}

#endif /* USE_SDL */
