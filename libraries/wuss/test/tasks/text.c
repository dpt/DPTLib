/* wuss/test/tasks/text.c -- static paragraph task */

#ifdef USE_SDL

#include <stdlib.h>

#include <math.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "geom/point.h"
#include "text/bmtext.h"

#include "text.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char paragraph[] =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
  "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

result_t text_create(wuss_t         *wuss,
                     const colour_t *palette,
                     bmfont_t       *font,
                     text_task_t    *task)
{
  wuss_task_t delegate;
  size2d_t    sz;
  result_t    rc;

  task->font        = font;
  task->bg          = palette[palette_PICO8_BLUE]; /* matches the "Lorem Ipsum" window's bg, for bmfont_draw's glyph blending */
  task->fg          = palette[palette_PICO8_WHITE];
  task->frame_count = 0;
  task->resizing    = true;

  delegate = wuss_task_start(text_handle, task);
  sz       = SIZE2D(220, 180);

  task->base_width  = sz.w;
  task->base_height = sz.h;

  rc = wuss_window_create_placed(wuss,
                                 sz,
                                 "Lorem Ipsum",
                                 wuss_WINDOW_NO_RESIZE_BLIT, /* paragraph reflows across the whole window, so a resize must redraw all of it, not just the newly (un)covered edge */
                                 wuss_BACKDROP_COLOUR(palette_PICO8_BLUE),
                                 &delegate,
                                 sz,
                                 SIZE2D(0, 0),
                                 &task->window);

  return rc;
}

#define INSET      4
#define LEADING    2
#define MAX_LINES  64 /* paragraph is short and fixed; overflow is dropped */

static result_t text_redraw(const wuss_event_t *event, void *task_data)
{
  text_task_t  *tcx;
  screen_t     *scr;
  const box_t  *bounds;
  int           sx, sy;
  bmtext_line_t lines[MAX_LINES];
  int           nlines;
  point_t       origin;

  tcx = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  nlines = bmtext_layout(tcx->font,
                         paragraph,
                         (int) strlen(paragraph),
                         (bounds->x1 - INSET) - (bounds->x0 + INSET),
                         lines,
                         MAX_LINES);

  origin.x = bounds->x0 - sx + INSET;
  origin.y = bounds->y0 - sy + INSET;

  bmtext_draw(tcx->font, scr, lines, nlines, tcx->fg, tcx->bg, LEADING, origin);

  return result_OK;
}

static result_t text_mouse(void *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  tcx->resizing = !tcx->resizing;

  return result_OK;
}

#define TEXT_RESIZE_PERIOD_FRAMES 300 /* one full swing every 5s at 60fps */
#define TEXT_RESIZE_AMPLITUDE     50  /* +/-50px either side of base_width */

static result_t text_idle(void *task_data)
{
  text_task_t *tcx;
  int          height, width;
  double       angle;
  result_t     rc;

  tcx = task_data;

  if (!tcx->resizing)
    return result_OK;

  height = tcx->base_height;

  tcx->frame_count++;
  angle = tcx->frame_count * (2.0 * M_PI / TEXT_RESIZE_PERIOD_FRAMES);
  width = tcx->base_width + (int) (TEXT_RESIZE_AMPLITUDE * sin(angle));

  rc = wuss_window_resize(tcx->window, SIZE2D(width, height));
  if (rc != result_OK)
    logf_warning("text_idle: wuss_window_resize(%d, %d) failed", width, height);

  return rc;
}

result_t text_handle(wuss_window_t      *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return text_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return text_mouse(task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_close(window);
    free(tcx); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  case wuss_EVENT_IDLE:
    return text_idle(task_data);

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
