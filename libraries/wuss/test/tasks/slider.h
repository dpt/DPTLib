/* wuss/test/tasks/slider.h -- slider icons task */

#ifndef TASKS_SLIDER_H
#define TASKS_SLIDER_H

#ifdef WUSS_APP

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/icon.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* demonstrates wuss_ICON_TYPE_SLIDER: a horizontal and a vertical slider, each
 * with its own min/max/default, and a label echoing whichever last moved */
typedef struct slider_task
{
  wuss_window_t *window;
  bmfont_t      *font;
  colour_t       label;
  wuss_icon_t   *horiz;
  wuss_icon_t   *vert;
  wuss_icon_t   *state; /* label echoing the last-moved slider's value */
}
slider_task_t;

wuss_window_fn_t slider_handle;

/* create the slider window against the given wuss instance */
result_t slider_create(wuss_t        *wuss,
                       bmfont_t      *font,
                       slider_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SLIDER_H */
