/* wuss/test/tasks/swatches.c -- fill-pattern swatch grid task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/pattern.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "wuss/icon.h"

#include "swatches.h"

/* ponytail: the app builds wuss with a 16-entry palette (apps/wuss/main.c)
 * and wuss exposes no count. Hardcode it; add a wuss_palette_count() only if
 * another palette size ever ships. */
#define SWATCHES_NCOLOURS 16
#define SWATCHES_CELL     16 /* px; cells butt together, no pitch */

#define SWATCHES_DOC_W (SWATCHES_NCOLOURS * SWATCHES_CELL)
#define SWATCHES_DOC_H (screen_PATTERN__LIMIT * SWATCHES_CELL)

#define SWATCHES_PAPER_RGB 0xFF, 0xFF, 0xFF

/* Redraw: plot one 4x4 PATTERN swatch per (pattern, colour) pair -- row =
 * pattern, column = palette index used as the pattern's ink over white. No
 * icons are retained; wuss_icon_plot resolves the palette live, so a palette
 * swap just redraws. */
static result_t swatches_redraw(swatches_task_t    *task,
                                const wuss_event_t *event)
{
  wuss_icon_spec_t spec;
  wuss_colour_t    paper;
  const box_t     *bounds;
  point_t          scroll;
  int              pat, col;
  result_t         rc;

  bounds = event->data.redraw.bounds;
  scroll = event->data.redraw.scroll;
  paper  = wuss_nearest_colour(task->wuss, SWATCHES_PAPER_RGB);

  memset(&spec, 0, sizeof(spec));
  spec.type = wuss_ICON_TYPE_PATTERN;
  spec.bg   = paper;

  for (pat = 0; pat < screen_PATTERN__LIMIT; pat++)
  {
    for (col = 0; col < SWATCHES_NCOLOURS; col++)
    {
      spec.bbox    = (box_t) BOX_POS_SIZE(col * SWATCHES_CELL,
                                          pat * SWATCHES_CELL,
                                          SWATCHES_CELL, SWATCHES_CELL);
      spec.fg      = (wuss_colour_t) col;
      spec.pattern = (screen_pattern_t) pat;

      rc = wuss_icon_plot(task->window, &spec, bounds, scroll);
      if (rc != result_OK)
        return rc;
    }
  }

  return result_OK;
}

result_t swatches_create(wuss_t *wuss, swatches_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  result_t    rc;

  task->wuss   = wuss;
  task->window = NULL;

  delegate_desc.handle    = swatches_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "swatches";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(SWATCHES_DOC_W, 140),
                                 "Swatches",
                                 wuss_WINDOW_NO_RESIZE_BLIT, /* grid spans the whole window; a resize redraws all of it */
                                 wuss_BACKDROP_COLOUR(wuss_nearest_colour(wuss, 0xDD, 0xDD, 0xDD)),
                                 SIZE2D(SWATCHES_DOC_W, SWATCHES_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  /* fully built: from here a last-window close reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  return result_OK;
}

/* A SELECT click on a cell makes that cell -- its fill pattern, its palette
 * colour as the ink, white paper -- the desktop backdrop. The grid has no
 * retained icons, so the cell is found by arithmetic on the click point
 * (virtual content space). */
static result_t swatches_click(swatches_task_t    *task,
                               const wuss_event_t *event)
{
  wuss_backdrop_t backdrop;
  point_t         pt;
  int             pat, col;

  if (event->data.mouse.action != wuss_MOUSE_DOWN ||
      !(event->data.mouse.button & wuss_BUTTON_SELECT))
    return result_OK;

  pt  = event->data.mouse.point;
  col = pt.x / SWATCHES_CELL;
  pat = pt.y / SWATCHES_CELL;

  if (pt.x < 0 || pt.y < 0 ||
      col >= SWATCHES_NCOLOURS || pat >= screen_PATTERN__LIMIT)
    return result_OK;

  backdrop = (wuss_backdrop_t) wuss_BACKDROP_PATTERN(
               (wuss_colour_t) col,
               (screen_pattern_t) pat,
               wuss_nearest_colour(task->wuss, SWATCHES_PAPER_RGB));
  return wuss_set_backdrop(task->wuss, &backdrop);
}

result_t swatches_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  swatches_task_t *task = task_data;

  NOT_USED(window);

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return swatches_redraw(task, event);

  case wuss_EVENT_MOUSE:
    return swatches_click(task, event);

  case wuss_EVENT_QUIT:
    free(task_data); /* calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
