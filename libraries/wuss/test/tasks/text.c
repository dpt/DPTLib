/* text.c -- wuss test - static paragraph task */

#ifdef USE_SDL

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
  box_t       box;
  result_t    rc;

  task->font        = font;
  task->bg          = palette[palette_PICO8_BLUE]; /* matches the "Lorem Ipsum" window's bg, for bmfont_draw's glyph blending */
  task->fg          = palette[palette_PICO8_WHITE];
  task->frame_count = 0;
  task->resizing    = true;

  delegate = wuss_task_start(text_handle, task, palette_PICO8_BLUE);
  box      = (box_t) BOX_POS_SIZE(120, 100, 220, 180);

  task->base_width = box.x1 - box.x0;

  rc = wuss_window_create(wuss, &box, "Lorem Ipsum", wuss_WINDOW_NONE, &delegate, &task->window);

  return rc;
}

void text_destroy(text_task_t *task)
{
  if (task->window != NULL)
    wuss_window_destroy(task->window);
}

static result_t text_redraw(screen_t *scr, const box_t *content, void *task_data)
{
  text_task_t *tcx;
  int             font_width, font_height;
  const char     *string;
  int             stringlen;
  point_t         pos;

  tcx = task_data;

  bmfont_get_info(tcx->font, &font_width, &font_height);

  string    = paragraph;
  stringlen = (int) strlen(paragraph);

  pos.x = content->x0 + 4;
  pos.y = content->y0 + 4;

  while (stringlen > 0 && pos.y + font_height <= content->y1)
  {
    int            absolute_break, friendly_break;
    bmfont_width_t width;

    bmfont_measure(tcx->font, string, stringlen, content->x1 - 4 - pos.x, &absolute_break, &width);

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

    bmfont_draw(tcx->font, scr, string, friendly_break, tcx->fg, tcx->bg, &pos, NULL);

    string    += friendly_break;
    stringlen -= friendly_break;
    while (stringlen > 0 && isspace((unsigned char) *string))
    {
      string++;
      stringlen--;
    }

    pos.x  = content->x0 + 4;
    pos.y += font_height + 2;
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
  box_t        visible;
  int          height, width;
  double       angle;
  result_t     rc;

  tcx = task_data;

  if (!tcx->resizing)
    return result_OK;

  wuss_window_get_content_bounds(tcx->window, &visible);
  height = visible.y1 - visible.y0;

  tcx->frame_count++;
  angle = tcx->frame_count * (2.0 * M_PI / TEXT_RESIZE_PERIOD_FRAMES);
  width = tcx->base_width + (int) (TEXT_RESIZE_AMPLITUDE * sin(angle));

  rc = wuss_window_resize(tcx->window, width, height);
  if (rc != result_OK)
    logf_warning("text_idle: wuss_window_resize(%d, %d) failed", width, height);

  return rc;
}

result_t text_handle(wuss_window_t     *window,
                     const wuss_event_t *event,
                     void               *task_data)
{
  text_task_t *tcx;

  tcx = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return text_redraw(event->data.redraw.scr, event->data.redraw.content, task_data);

  case wuss_EVENT_MOUSE:
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return text_mouse(task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_destroy(window);
    tcx->window = NULL;
    return result_OK;

  case wuss_EVENT_IDLE:
    return text_idle(task_data);

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
