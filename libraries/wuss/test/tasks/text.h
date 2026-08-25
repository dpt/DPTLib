/* text.h -- wuss test - static paragraph task */

#ifndef TASKS_TEXT_H
#define TASKS_TEXT_H

#ifdef USE_SDL

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
  int            base_width;  /* visible width when the window was created */
  int            frame_count;
}
text_task_t;

wuss_redraw_fn_t text_redraw;

/* create the paragraph-of-text window against the given wuss instance */
result_t text_create(wuss_t *wuss, const colour_t *palette, bmfont_t *font, text_task_t *task);

/* destroy the paragraph-of-text window created by text_create */
void text_destroy(text_task_t *task);

/* slowly resize the window's width +/-50px around base_width, holding
 * height fixed; called once per frame from the main loop */
void text_step(text_task_t *tcx);

#endif /* USE_SDL */

#endif /* TASKS_TEXT_H */
