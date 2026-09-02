/* wuss/test/tasks/chars.h -- system font glyph grid task */

#ifndef TASKS_CHARS_H
#define TASKS_CHARS_H

#ifdef USE_SDL

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/window.h"

/* displays every glyph (0-255) of the wuss system font as a 32x8 grid,
 * so the whole font can be eyeballed at a glance */
typedef struct chars_task
{
  wuss_window_t *window;
  bmfont_t      *font;
  colour_t       fg, bg;
}
chars_task_t;

wuss_window_fn_t chars_handle;

/* create the glyph-grid window against the given wuss instance; does
 * nothing and returns result_OK if wuss has no system font */
result_t chars_create(wuss_t *wuss, chars_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_CHARS_H */
