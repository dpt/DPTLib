/* icons.c -- wuss test - work-area icons task */

#ifdef USE_SDL
#include "framebuf/palettes.h"

#include <stdio.h>
#include <stdlib.h>
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
  /* [0..3], one swatch per built-in pattern, then [.. +3] a grouping frame
   * with two differently-justified labels inside it */
  enum { ICONS_NSPECS = 4 + screen_PATTERN__LIMIT + 3 };
  wuss_task_t      delegate;
  wuss_icon_spec_t specs[ICONS_NSPECS];
  wuss_icon_t     *made[ICONS_NSPECS];
  int              p;
  int              g;   /* index of the first frame spec */
  result_t         rc;

  task->font    = font;
  task->label   = palette[palette_PICO8_DARK_BLUE];
  task->paper   = palette[palette_PICO8_LIGHT_GREY]; /* the window bg, below */
  task->window  = NULL;
  task->button  = NULL;
  task->counter = NULL;
  task->count   = 0;

  delegate = wuss_task_start(icons_handle, task);

  rc = wuss_window_create_placed(wuss,
                                 SIZE2D(ICONS_DOC_W, 160),
                                 "Icons",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_PATTERN(palette_PICO8_LAVENDER,
                                                       screen_PATTERN_CROSSHATCH,
                                                       palette_PICO8_LIGHT_GREY),
                                 &delegate,
                                 SIZE2D(ICONS_DOC_W, ICONS_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    return rc;

  memset(specs, 0, sizeof(specs));

  /* icons sit past the ruler gutter (see ICONS_GUTTER in icons_redraw) so the
   * axis labels have the top/left strip to themselves */

  /* [0] a heading label */
  specs[0].bbox = (box_t) BOX_POS_SIZE(28, 28, 180, 14);
  specs[0].type = wuss_ICON_TYPE_LABEL;
  specs[0].text = "Work-area icons:";
  specs[0].fg   = palette_PICO8_DARK_BLUE;
  specs[0].bg   = wuss_NO_BACKGROUND;

  /* [1] the button that bumps the counter */
  specs[1].bbox = (box_t) BOX_POS_SIZE(28, 50, 80, 22);
  specs[1].type = wuss_ICON_TYPE_BUTTON;
  specs[1].text = "Press me";
  specs[1].fg   = palette_PICO8_BLACK;
  specs[1].bg   = palette_PICO8_LIGHT_GREY;

  /* [2] the counter label beside it */
  specs[2].bbox = (box_t) BOX_POS_SIZE(120, 50, 120, 22);
  specs[2].type = wuss_ICON_TYPE_LABEL;
  specs[2].text = "0";
  specs[2].fg   = palette_PICO8_DARK_BLUE;
  specs[2].bg   = wuss_NO_BACKGROUND;

  /* [3] a button far down the document, to prove icons scroll and stay clickable */
  specs[3].bbox = (box_t) BOX_POS_SIZE(28, 460, 90, 52);
  specs[3].type = wuss_ICON_TYPE_BUTTON;
  specs[3].text = "Scrolled";
  specs[3].fg   = palette_PICO8_BLACK;
  specs[3].bg   = palette_PICO8_LIGHT_GREY;

  /* [4..] one swatch per built-in pattern, in a column down the document so
   * they scroll through the window and stay phase-locked while doing so */
  for (p = 0; p < screen_PATTERN__LIMIT; p++)
  {
    specs[4 + p].bbox    = (box_t) BOX_POS_SIZE(28, 90 + p * 44, 90, 36);
    specs[4 + p].type    = wuss_ICON_TYPE_PATTERN;
    specs[4 + p].fg      = palette_PICO8_DARK_BLUE;
    specs[4 + p].bg      = palette_PICO8_LIGHT_GREY;
    specs[4 + p].pattern = (screen_pattern_t) p;
  }

  /* [g] a grouping frame down the document, with [g+1] a right-justified and
   * [g+2] a centred label sat inside it */
  g = 4 + screen_PATTERN__LIMIT;

  specs[g].bbox = (box_t) BOX_POS_SIZE(28, 300, 170, 70);
  specs[g].type = wuss_ICON_TYPE_FRAME;
  specs[g].text = "Grouping frame";
  specs[g].fg   = palette_PICO8_DARK_BLUE;
  specs[g].bg   = wuss_NO_BACKGROUND;

  specs[g + 1].bbox  = (box_t) BOX_POS_SIZE(38, 320, 150, 14);
  specs[g + 1].type  = wuss_ICON_TYPE_LABEL;
  specs[g + 1].text  = "right";
  specs[g + 1].fg    = palette_PICO8_DARK_BLUE;
  specs[g + 1].bg    = wuss_NO_BACKGROUND;
  specs[g + 1].flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;

  specs[g + 2].bbox  = (box_t) BOX_POS_SIZE(38, 342, 150, 14);
  specs[g + 2].type  = wuss_ICON_TYPE_LABEL;
  specs[g + 2].text  = "centre";
  specs[g + 2].fg    = palette_PICO8_DARK_BLUE;
  specs[g + 2].bg    = wuss_NO_BACKGROUND;
  specs[g + 2].flags = wuss_ICON_FLAGS_JUSTIFY_CENTRE;

  rc = wuss_icon_create_array(task->window, specs, ICONS_NSPECS, made);
  if (rc != result_OK)
    goto failure;

  task->button  = made[1];
  task->counter = made[2];

  return result_OK;

failure:
  wuss_window_close(task->window);
  task->window = NULL;
  return rc;
}

#define ICONS_GRID       16 /* document-space pitch of the backdrop grid */
#define ICONS_AXIS_LABEL 64 /* label every Nth grid line along each axis */

/* Screen coordinate of the first grid line at or after lo. Grid lines sit at
 * document multiples of ICONS_GRID; the screen coordinate of document d is
 * origin - scroll + d, so lines fall on screen coordinates congruent to
 * (origin - scroll) modulo ICONS_GRID. */
static int icons_grid_first(int origin, int scroll, int lo)
{
  int phase;

  phase = (origin - scroll) % ICONS_GRID;
  if (phase < 0)
    phase += ICONS_GRID;

  return lo - ((lo - phase) % ICONS_GRID + ICONS_GRID) % ICONS_GRID;
}

static result_t icons_redraw(const wuss_event_t *event, void *task_data)
{
  icons_task_t *tcx;
  screen_t     *scr;
  const box_t  *content;
  const box_t  *bounds;
  point_t       scroll;
  point_t       pos;
  char          buf[16];
  int           ox;     /* screen x of document x=0 */
  int           oy;     /* screen y of document y=0 */
  int           doc;
  int           x;
  int           y;

  tcx = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  scroll  = event->data.redraw.scroll;

  ox = bounds->x0 - scroll.x;
  oy = bounds->y0 - scroll.y;

  /* Everything this task paints is anchored to the document, not the window,
   * so it scrolls rigidly with the content -- which is what Wuss's scroll
   * blit assumes. Nothing here is pinned to a window edge. Every draw is
   * clipped to the dirty rectangle (content) so partial redraws stay cheap. */

  /* the faint crosshatch backdrop is now the window's own bg (a
   * wuss_backdrop_t pattern fill), painted by Wuss before this event and
   * phase-locked to the scroll origin -- nothing to draw here */

  /* x-axis ruler: document x printed just below the y=0 line, at each
   * labelled grid column. Scrolls with the document like the grid. */
  for (x = icons_grid_first(bounds->x0, scroll.x, content->x0);
       x < content->x1;
       x += ICONS_GRID)
  {
    doc = x - ox;
    if (doc <= 0 || doc % ICONS_AXIS_LABEL != 0)
      continue;

    snprintf(buf, sizeof(buf), "%d", doc);
    pos = POINT(x + 2, oy + 2);
    bmfont_draw(tcx->font, scr, buf, (int) strlen(buf),
                tcx->label, tcx->paper, &pos, NULL);
  }

  /* y-axis ruler: document y printed just right of the x=0 line, at each
   * labelled grid row. */
  for (y = icons_grid_first(bounds->y0, scroll.y, content->y0);
       y < content->y1;
       y += ICONS_GRID)
  {
    doc = y - oy;
    if (doc <= 0 || doc % ICONS_AXIS_LABEL != 0)
      continue;

    snprintf(buf, sizeof(buf), "%d", doc);
    pos = POINT(ox + 2, y + 2);
    bmfont_draw(tcx->font, scr, buf, (int) strlen(buf),
                tcx->label, tcx->paper, &pos, NULL);
  }

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
    free(tcx); /* calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
