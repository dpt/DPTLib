/* create.c -- wuss - minimal window manager */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

result_t wuss_window_create(wuss_t             *wuss,
                            const box_t        *content,
                            const char         *title,
                            wuss_window_flags_t flags,
                            const wuss_task_t  *task,
                            int                 doc_width,
                            int                 doc_height,
                            wuss_window_t     **window)
{
  wuss_window_t *win;
  int             width, height, outline_px, titlebar_height, carve_x, carve_y;
  int             scr_width, scr_height, dx, dy;

  assert(wuss    != NULL);
  assert(content != NULL);
  assert(window  != NULL);

  width  = content->x1 - content->x0;
  height = content->y1 - content->y0;
  if (!wuss__size_ok(width, height))
    return result_WUSS_TOO_SMALL;

  win = malloc(sizeof(*win));
  if (win == NULL)
    return result_OOM;

  outline_px      = wuss__outline_px_for(flags);
  titlebar_height = wuss__titlebar_height_for(wuss, flags);
  wuss__furniture_carve_for(flags, wuss__icon_size_for(wuss, flags), &carve_x, &carve_y);

  win->wuss       = wuss;
  win->visible.x0 = content->x0 - outline_px;
  win->visible.y0 = content->y0 - outline_px - titlebar_height;
  win->visible.x1 = content->x1 + outline_px + carve_x;
  win->visible.y1 = content->y1 + outline_px + carve_y;

  /* nudge back on-screen so the titlebar/close icon stay reachable; a
   * window bigger than the screen keeps its top-left (titlebar) edge
   * on-screen rather than being centred or left alone */
  scr_width  = wuss->scr->width;
  scr_height = wuss->scr->height;

  dx = 0;
  if (win->visible.x0 < 0)
    dx = -win->visible.x0;
  else if (win->visible.x1 > scr_width)
    dx = scr_width - win->visible.x1;
  if (win->visible.x0 + dx < 0)
    dx = -win->visible.x0;

  dy = 0;
  if (win->visible.y0 < 0)
    dy = -win->visible.y0;
  else if (win->visible.y1 > scr_height)
    dy = scr_height - win->visible.y1;
  if (win->visible.y0 + dy < 0)
    dy = -win->visible.y0;

  win->visible.x0 += dx; win->visible.x1 += dx;
  win->visible.y0 += dy; win->visible.y1 += dy;

  win->flags      = flags;
  win->scroll_x   = 0;
  win->scroll_y   = 0;
  win->doc_width  = doc_width;
  win->doc_height = doc_height;
  win->toggled    = 0;

  if (task != NULL)
    win->task = *task;
  else
    memset(&win->task, 0, sizeof(win->task));

  if (task == NULL)
    win->task.bg = wuss_NO_BACKGROUND;
  else if (task->bg != wuss_NO_BACKGROUND &&
           (task->bg < 0 || task->bg >= wuss->npalette))
  {
    free(win);
    return result_WUSS_BAD_COLOUR;
  }

  if (title != NULL)
  {
    strncpy(win->title, title, WUSS_TITLE_MAX);
    win->title[WUSS_TITLE_MAX] = '\0';
  }
  else
  {
    win->title[0] = '\0';
  }

  list_add_to_head(&wuss->z_order, &win->link);

  wuss_invalidate(wuss, &win->visible);

  *window = win;

  return result_OK;
}
