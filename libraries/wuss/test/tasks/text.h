/* wuss/test/tasks/text.h -- static paragraph task */

#ifndef TASKS_TEXT_H
#define TASKS_TEXT_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/fontmenu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* window B's task: flows a fixed paragraph of placeholder text over its
 * wuss-filled background, one line per bmfont_draw call. A MENU click on the
 * window opens a picker -- a wuss_fontmenu over resources/bmfonts -- that
 * swaps the paragraph font in place. */
typedef struct text_task
{
  wuss_window_t   *window;
  wuss_task_t     *delegate;   /* the wuss task backing this window */
  wuss_t          *wuss;       /* for wuss_get_pointer when opening the menu */
  wuss_fontmenu_t *fontmenu;   /* the font picker; owns the menu and names */
  bmfont_t        *font;       /* currently shown; fonts[current] or sysfont */
  bmfont_t       **fonts;      /* one slot per menu item, lazily loaded */
  int              nfonts;     /* length of fonts[]; == menu item count */
  int              current;    /* index into fonts[], or -1 for sysfont */
  colour_t         bg, fg;
  char             resources[256]; /* root the picker loads bmfonts from */
  int              base_width;  /* content width when the window was made */
  int              base_height; /* content height when the window was made */
  int              frame_count;
  bool             resizing;    /* toggled by a content click; text_step only
                                 * resizes the window while this is true */
}
text_task_t;

wuss_window_fn_t text_handle;

/* create the paragraph-of-text window against the given wuss instance.
 * resources is the root the font picker loads bmfonts from
 * (resources/bmfonts/<name>.png). */
result_t text_create(wuss_t         *wuss,
                     const char     *resources,
                     text_task_t    *task);

#endif /* WUSS_APP */

#endif /* TASKS_TEXT_H */
