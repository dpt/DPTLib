/* wuss/test/tasks/text.h -- sample-text task */

#ifndef TASKS_TEXT_H
#define TASKS_TEXT_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/fontmenu.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* window B's task: flows a chosen sample string over its wuss-filled
 * background, one line per bmfont_draw call. A MENU click on the window opens
 * a top-level menu with two submenus: "Font" -- a wuss_fontmenu over
 * resources/bmfonts, swapping the paragraph font in place -- and "Sample",
 * which swaps the shown string (a choice of pangrams and a lorem ipsum
 * paragraph). */
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
  wuss_menu_item_t top_items[2]; /* "Font" and "Sample", built once the
                                 * fontmenu exists so items[0].submenu can
                                 * borrow its live wuss_menu_t */
  wuss_menu_t      top_menu;   /* root menu passed to wuss_menu_open */
  int              sample;     /* index into text_samples[] currently shown */
  const char      *text;       /* text_samples[sample].text; what text_redraw
                                 * lays out and draws */
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

/* create the sample-text window against the given wuss instance.
 * resources is the root the font picker loads bmfonts from
 * (resources/bmfonts/<name>.png). */
result_t text_create(wuss_t         *wuss,
                     const char     *resources,
                     text_task_t    *task);

#endif /* WUSS_APP */

#endif /* TASKS_TEXT_H */
