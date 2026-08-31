/* text.c -- wuss test - static paragraph task */

#ifdef USE_SDL

#include <stdlib.h>

#include <ctype.h>
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

static result_t text_redraw(const wuss_event_t *event, void *task_data)
{
  text_task_t *tcx;
  screen_t       *scr;
  const box_t    *bounds;
  int             font_width, font_height, sx, sy;
  const char     *string;
  int             stringlen;
  point_t         pos0, pos1;

  tcx = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  bmfont_get_info(tcx->font, &font_width, &font_height);

  string    = paragraph;
  stringlen = (int) strlen(paragraph);

#define INSET   4
#define LEADING 2
  
  pos0.x = bounds->x0 - sx + INSET;
  pos0.y = bounds->y0 - sy + INSET;
  pos1.x = bounds->x1 - sx - INSET;
  pos1.y = bounds->y1 - sy - INSET;

  while (stringlen)
  {
    int            target_width;
    int            absolute_break;
    bmfont_width_t width;
    int            friendly_break;

    target_width = pos1.x - pos0.x;
    bmfont_measure(tcx->font, string, stringlen, target_width, &absolute_break, &width);

    friendly_break = absolute_break;
    if (absolute_break < stringlen)
    {
      /* line didn't fit whole: try to break at the last space within it */
      for (friendly_break = absolute_break - 1; friendly_break > 0; friendly_break--)
        if (isspace((unsigned char) string[friendly_break]))
          break;
      if (friendly_break <= 0)
        friendly_break = absolute_break; /* no space to break at: hard break */
    }

    bmfont_draw(tcx->font, scr, string, friendly_break, tcx->fg, tcx->bg, &pos0, NULL);

    string    += friendly_break;
    stringlen -= friendly_break;
    while (stringlen > 0 && isspace((unsigned char) *string))
    {
      string++;
      stringlen--;
    }

    pos0.y += font_height + LEADING;
  }

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
