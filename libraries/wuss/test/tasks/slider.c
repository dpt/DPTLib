/* wuss/test/tasks/slider.c -- slider icons task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "slider.h"

#define SLIDER_DOC_W  240
#define SLIDER_DOC_H  200
#define SLIDER_MARGIN 20

enum { SLIDER_NSPECS = 3 }; /* horizontal slider, vertical slider, state label */

result_t slider_create(wuss_t        *wuss,
                       bmfont_t      *font,
                       slider_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  wuss_icon_spec_t specs[SLIDER_NSPECS];
  wuss_icon_t     *made[SLIDER_NSPECS];
  result_t         rc;

  task->font  = font;
  task->label = colour_rgb(0x00, 0x00, 0x00);
  task->window = NULL;
  task->horiz  = NULL;
  task->vert   = NULL;
  task->state  = NULL;

  delegate_desc.handle    = slider_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "slider";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(SLIDER_DOC_W, SLIDER_DOC_H),
                                 "Sliders",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 SIZE2D(SLIDER_DOC_W, SLIDER_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the task block */
    return rc;
  }

  memset(specs, 0, sizeof(specs));

  specs[0].bbox        = (box_t) BOX_POS_SIZE(SLIDER_MARGIN, 30,
                                              SLIDER_DOC_W - SLIDER_MARGIN * 2,
                                              20);
  specs[0].type        = wuss_ICON_TYPE_SLIDER;
  specs[0].fg          = wuss_COLOUR_BLACK;
  specs[0].bg          = wuss_NO_BACKGROUND;
  specs[0].u.slider.orientation    = wuss_SLIDER_HORIZONTAL;
  specs[0].u.slider.min            = 0;
  specs[0].u.slider.max            = 100;
  specs[0].u.slider.default_value  = 25;

  specs[1].bbox        = (box_t) BOX_POS_SIZE(SLIDER_DOC_W - SLIDER_MARGIN - 20,
                                              70, 20, 100);
  specs[1].type        = wuss_ICON_TYPE_SLIDER;
  specs[1].fg          = wuss_COLOUR_BLACK;
  specs[1].bg          = wuss_NO_BACKGROUND;
  specs[1].u.slider.orientation    = wuss_SLIDER_VERTICAL;
  specs[1].u.slider.min            = 0;
  specs[1].u.slider.max            = 10;
  specs[1].u.slider.default_value  = 10;

  specs[2].bbox = (box_t) BOX_POS_SIZE(SLIDER_MARGIN, 70, 150, 14);
  specs[2].type = wuss_ICON_TYPE_LABEL;
  specs[2].text = "horizontal: 25";
  specs[2].fg   = wuss_COLOUR_BLACK;
  specs[2].bg   = wuss_NO_BACKGROUND;

  rc = wuss_icon_create_array(task->window, specs, SLIDER_NSPECS, made);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* closes the window; QUIT frees the block */
    return rc;
  }

  task->horiz = made[0];
  task->vert  = made[1];
  task->state = made[2];

  wuss_task_set_autoclose(delegate, 1);

  return result_OK;
}

static result_t slider_icon(const wuss_event_t *event, void *task_data)
{
  slider_task_t *tcx;
  wuss_icon_t   *icon;
  const char    *name;
  char           buf[48];

  tcx  = task_data;
  icon = event->data.icon.icon;

  if (icon != tcx->horiz && icon != tcx->vert)
    return result_OK;

  name = (icon == tcx->horiz) ? "horizontal" : "vertical";
  snprintf(buf, sizeof(buf), "%s: %d", name, event->data.icon.value);

  return wuss_icon_set_text(tcx->window, tcx->state, buf);
}

result_t slider_handle(wuss_window_t      *window,
                       const wuss_event_t *event,
                       void               *task_data)
{
  (void) window;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    return slider_icon(event, task_data);

  case wuss_EVENT_QUIT:
    free(task_data); /* calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
