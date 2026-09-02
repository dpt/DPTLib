/* wuss/test/tasks/palette.c -- desktop palette swatch grid task */

#ifdef WUSS_APP

#include <stdlib.h>

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
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t         rc;

  task->palette  = palette;
  task->npalette = npalette;

  /* backdrop for any rounding gap around the grid */
  delegate_desc.handle    = palette_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "palette";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(100, 100),
                                 "Palette",
                                 wuss_WINDOW_NO_RESIZE_BLIT, /* swatch grid is laid out across the whole window, so a resize must redraw all of it, not just the newly (un)covered edge */
                                 wuss_BACKDROP_COLOUR(palette_PICO8_BLACK),
                                 SIZE2D(100, 100),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */

  return rc;
}

static result_t palette_redraw(const wuss_event_t *event, void *task_data)
{
  palette_task_t *pc;
  screen_t       *scr;
  const box_t    *bounds;
  int               cols, rows;
  int               cell_w, cell_h;
  int               i, sx, sy;

  pc = task_data;

  if (pc->npalette <= 0)
    return result_OK;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  cols = 1;
  while (cols * cols < pc->npalette)
    cols++;
  rows = (pc->npalette + cols - 1) / cols;

  cell_w = (bounds->x1 - bounds->x0) / cols;
  cell_h = (bounds->y1 - bounds->y0) / rows;

  for (i = 0; i < pc->npalette; i++)
  {
    int col, row, x, y;

    col = i % cols;
    row = i / cols;
    x   = bounds->x0 - sx + col * cell_w;
    y   = bounds->y0 - sy + row * cell_h;

    screen_fill_rect(scr, x, y, SIZE2D(cell_w, cell_h), pc->palette[i]);
  }

  return result_OK;
}

result_t palette_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  if (event->kind == wuss_EVENT_QUIT)
  {
    free(task_data); /* calloc'd per instance by the spawner */
    return result_OK;
  }

  if (event->kind != wuss_EVENT_REDRAW)
    return result_OK;

  return palette_redraw(event, task_data);
}

#endif /* WUSS_APP */
