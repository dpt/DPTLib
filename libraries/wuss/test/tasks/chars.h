/* wuss/test/tasks/chars.h -- system font glyph grid task */

#ifndef TASKS_CHARS_H
#define TASKS_CHARS_H

#ifdef WUSS_APP

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* number of fonts offered by the Chars font-picker menu */
#define CHARS_NFONTS 7

/* displays every glyph (0-255) of a bitmap font as a 32x8 grid, so the whole
 * font can be eyeballed at a glance. A MENU click on the window opens a
 * picker that swaps the font in place. */
typedef struct chars_task
{
  wuss_window_t *window;
  wuss_task_t   *delegate;    /* the wuss task backing this window */
  wuss_t        *wuss;        /* for wuss_get_pointer when opening the menu */
  bmfont_t      *font;        /* currently shown; == fonts[current] */
  bmfont_t      *fonts[CHARS_NFONTS]; /* lazily loaded, NULL until first pick */
  int            current;    /* index into fonts[] */
  colour_t       fg, bg;
}
chars_task_t;

wuss_window_fn_t chars_handle;

/* create the glyph-grid window against the given wuss instance; does
 * nothing and returns result_OK if wuss has no system font. resources is the
 * root the font picker loads bmfonts from (resources/bmfonts/<name>.png). */
result_t chars_create(wuss_t       *wuss,
                      const char   *resources,
                      chars_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CHARS_H */
