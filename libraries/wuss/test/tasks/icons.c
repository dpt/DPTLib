/* icons.c -- wuss test - work-area icons task */

#ifdef USE_SDL
#include "framebuf/palettes.h"

#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "icons.h"

#define ICONS_DOC_W 220
#define ICONS_DOC_H 520 /* taller than the window, so scrolling is exercised */

result_t icons_create(wuss_t         *wuss,
                      const colour_t *palette,
                      bmfont_t       *font,
                      icons_task_t   *task)
{
  wuss_task_t      delegate;
  box_t            box;
  wuss_icon_spec_t spec;
  result_t         rc;

  task->font    = font;
  task->ink     = palette[palette_PICO8_LAVENDER];
  task->window  = NULL;
  task->button  = NULL;
  task->counter = NULL;
  task->count   = 0;

  delegate = wuss_task_start(icons_handle, task);
  box      = (box_t) BOX_POS_SIZE(160, 120, ICONS_DOC_W, 160);

  rc = wuss_window_create(wuss,
                          &box,
                          "Icons",
                          wuss_WINDOW_NONE,
                          palette_PICO8_LIGHT_GREY,
                          &delegate,
                          SIZE2D(ICONS_DOC_W, ICONS_DOC_H),
                          SIZE2D(0, 0),
                          &task->window);
  if (rc != result_OK)
    return rc;

  memset(&spec, 0, sizeof(spec));

  /* a heading label */
  spec.bbox  = (box_t) BOX_POS_SIZE(8, 8, 180, 14);
  spec.type  = wuss_ICON_TYPE_LABEL;
  spec.text  = "Work-area icons:";
  spec.fg    = palette_PICO8_DARK_BLUE;
  spec.bg    = wuss_NO_BACKGROUND;
  spec.flags = wuss_ICON_FLAGS_NONE;
  rc = wuss_icon_create(task->window, &spec, NULL);
  if (rc != result_OK)
    goto failure;

  /* the button that bumps the counter */
  spec.bbox = (box_t) BOX_POS_SIZE(8, 30, 80, 22);
  spec.type = wuss_ICON_TYPE_BUTTON;
  spec.text = "Press me";
  spec.fg   = palette_PICO8_BLACK;
  spec.bg   = palette_PICO8_LIGHT_GREY;
  rc = wuss_icon_create(task->window, &spec, &task->button);
  if (rc != result_OK)
    goto failure;

  /* the counter label beside it */
  spec.bbox = (box_t) BOX_POS_SIZE(100, 30, 140, 52);
  spec.type = wuss_ICON_TYPE_LABEL;
  spec.text = "0";
  spec.fg   = palette_PICO8_DARK_BLUE;
  spec.bg   = wuss_NO_BACKGROUND;
  rc = wuss_icon_create(task->window, &spec, &task->counter);
  if (rc != result_OK)
    goto failure;

  /* a button far down the document, to prove icons scroll and stay clickable */
  spec.bbox = (box_t) BOX_POS_SIZE(8, 460, 90, 52);
  spec.type = wuss_ICON_TYPE_BUTTON;
  spec.text = "Scrolled";
  spec.fg   = palette_PICO8_BLACK;
  spec.bg   = palette_PICO8_LIGHT_GREY;
  rc = wuss_icon_create(task->window, &spec, NULL);
  if (rc != result_OK)
    goto failure;

  return result_OK;

failure:
  wuss_window_close(task->window);
  task->window = NULL;
  return rc;
}

void icons_destroy(icons_task_t *task)
{
  wuss_window_close(task->window);
}

static result_t icons_redraw(const wuss_event_t *event, void *task_data)
{
  icons_task_t *tcx;
  screen_t     *scr;
  const box_t  *content;
  const box_t  *bounds;
  point_t       scroll;
  int           phase;
  int           first;
  int           x;

  tcx = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  scroll  = event->data.redraw.scroll;

  /* faint vertical rules every 16 document units, to show the task still
   * paints under and around the wuss-managed icons -- anchored to the
   * document so they track the scroll offset. The screen x of document
   * x=d is bounds->x0 - scroll.x + d, so the rules land on screen
   * columns congruent to (bounds->x0 - scroll.x) modulo 16. */
  phase = (bounds->x0 - scroll.x) % 16;
  if (phase < 0)
    phase += 16;
  first = content->x0 - ((content->x0 - phase) % 16 + 16) % 16;
  for (x = first; x < content->x1; x += 16)
    screen_draw_line(scr, x, content->y0, x, content->y1 - 1, tcx->ink);

  return result_OK;
}

static result_t icons_icon(const wuss_event_t *event, void *task_data)
{
  icons_task_t *tcx;
  char          buf[16];

  tcx = task_data;

  if (event->data.icon.action != wuss_MOUSE_DOWN)
    return result_OK;
  if (event->data.icon.icon != tcx->button)
    return result_OK;

  tcx->count++;
  snprintf(buf, sizeof(buf), "%d", tcx->count);

  return wuss_icon_set_text(tcx->counter, buf);
}

result_t icons_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  icons_task_t *tcx;

  tcx = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return icons_redraw(event, task_data);

  case wuss_EVENT_ICON:
    return icons_icon(event, task_data);

  case wuss_EVENT_CLOSE:
    wuss_window_close(window);
    tcx->window = NULL;
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
