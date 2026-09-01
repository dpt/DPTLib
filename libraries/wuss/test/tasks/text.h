/* wuss/test/tasks/text.h -- static paragraph task */

#ifndef TASKS_TEXT_H
#define TASKS_TEXT_H

#ifdef USE_SDL

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/window.h"

/* window B's task: flows a fixed paragraph of placeholder text over its
 * wuss-filled background, one line per bmfont_draw call */
typedef struct text_task
{
  wuss_window_t *window;
  bmfont_t      *font;
  colour_t       bg, fg;
  int            base_width;  /* content width when the window was created */
  int            base_height; /* content height when the window was created */
  int            frame_count;
  bool           resizing;    /* toggled by a content click; text_step only
                                * resizes the window while this is true */
}
text_task_t;

wuss_event_fn_t text_handle;

/* create the paragraph-of-text window against the given wuss instance */
result_t text_create(wuss_t         *wuss,
                     bmfont_t       *font,
                     text_task_t    *task);

#endif /* USE_SDL */

#endif /* TASKS_TEXT_H */
