/* wuss/test/tasks/swatches.c -- fill-pattern swatch grid task */

#ifdef USE_SDL

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/size.h"
#include "wuss/icon.h"

#include "swatches.h"

#define SWATCHES_DOC_W 160
#define SWATCHES_DOC_H 320 /* taller than the window, so scrolling is exercised */

result_t swatches_create(wuss_t *wuss, swatches_task_t *task)
{
  enum { SWATCHES_NSPECS = 1 + screen_PATTERN__LIMIT };
  wuss_task_t      delegate;
  wuss_icon_spec_t specs[SWATCHES_NSPECS];
  int              p;
  wuss_colour_t    fg, bg;
  result_t         rc;

  fg = wuss_nearest_colour(wuss, 0x00, 0x00, 0x00);
  bg = wuss_nearest_colour(wuss, 0xFF, 0xFF, 0xFF);

  task->window = NULL;

  delegate = wuss_task_start(swatches_handle, task);

  rc = wuss_window_create_placed(wuss,
                                 SIZE2D(SWATCHES_DOC_W, 140),
                                 "Swatches",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_COLOUR(wuss_nearest_colour(wuss, 0xDD, 0xDD, 0xDD)),
                                 &delegate,
                                 SIZE2D(SWATCHES_DOC_W, SWATCHES_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    return rc;

  memset(specs, 0, sizeof(specs));

  /* [0] a heading label */
  specs[0].bbox = (box_t) BOX_POS_SIZE(12, 12, 140, 14);
  specs[0].type = wuss_ICON_TYPE_LABEL;
  specs[0].text = "Fill patterns:";
  specs[0].fg   = fg;
  specs[0].bg   = wuss_NO_BACKGROUND;

  /* [1..] one 16x16 swatch per built-in fill pattern, laid out as a grid down
   * the document so it scrolls through the window and stays phase-locked while
   * doing so */
  for (p = 0; p < screen_PATTERN__LIMIT; p++)
  {
    enum { SWATCHES_COLS = 6, SWATCHES_PITCH = 20 };
    int col, row;

    col = p % SWATCHES_COLS;
    row = p / SWATCHES_COLS;

    specs[1 + p].bbox    = (box_t) BOX_POS_SIZE(12 + col * SWATCHES_PITCH,
                                                34 + row * SWATCHES_PITCH,
                                                16, 16);
    specs[1 + p].type    = wuss_ICON_TYPE_PATTERN;
    specs[1 + p].fg      = fg;
    specs[1 + p].bg      = bg;
    specs[1 + p].pattern = (screen_pattern_t) p;
  }

  rc = wuss_icon_create_array(task->window, specs, SWATCHES_NSPECS, NULL);
  if (rc != result_OK)
  {
    wuss_window_close(task->window);
    task->window = NULL;
    return rc;
  }

  return result_OK;
}

result_t swatches_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  if (event->kind == wuss_EVENT_CLOSE)
  {
    wuss_window_close(window);
    free(task_data); /* calloc'd per instance by the spawner */
    return result_OK;
  }

  return result_OK;
}

#endif /* USE_SDL */
