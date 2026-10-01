/* wuss/test/tasks/chars.h -- system font glyph grid task */

#ifndef TASKS_CHARS_H
#define TASKS_CHARS_H

#ifdef WUSS_APP

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/fontmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/component/saveas.h"
#include "wuss/task.h"
#include "wuss/window.h"

#define CHARS_MAX_PAGES 16 /* "Page" submenu rows; pages past this are not
                            * offered */

/* displays one 256-codepoint Unicode page of a bitmap font as a 16x16 grid,
 * so the font can be eyeballed at a glance. A MENU click on the window opens
 * a menu whose "Font" row is the shared wuss_fontmenu singleton over
 * resources/bmfonts, swapping the font in place, and whose "Page" row lists
 * the pages the current font has glyphs in. Select and Adjust clicks step
 * to the next and previous of those pages. Menu > Save PNG opens a Save As
 * dialogue (drag its icon onto a Filer window, or Save with a full path
 * already typed) that writes the grid out. */
typedef struct chars_task
{
  wuss_window_t      *window;
  wuss_task_t        *delegate;    /* the wuss task backing this window */
  wuss_saveas_t       *saveas;
  wuss_t             *wuss;        /* for wuss_get_pointer when opening the
                                    * menu */
  wuss_menu_handle_t  menu_handle; /* the open chain, to re-tick it live on
                                    * an ADJUST pick that keeps it open */
  bmfont_t           *font;        /* currently shown; == fonts[current]
                                    * once loaded */
  bmfont_t          **fonts;       /* one slot per menu item, lazily loaded */
  int                 nfonts;      /* length of fonts[]; == menu item count */
  int                 current;     /* index into fonts[], or -1 if the
                                    * system font is not listed */
  colour_t            fg, mg, bg;
  unsigned long       page;        /* first codepoint of the page shown */
  wuss_menu_item_t    page_items[CHARS_MAX_PAGES]; /* rebuilt each time
                                    * "Page" opens, for the current font */
  char                page_labels[CHARS_MAX_PAGES][8]; /* "U+1F400" */
  unsigned long       page_bases[CHARS_MAX_PAGES]; /* page_items[i]'s page */
  wuss_menu_t         page_menu;
  wuss_menu_item_t    menu_items[4]; /* per-instance: a shared static would
                                      * leak one instance's .window pointer
                                      * into another's menu */
  wuss_menu_t         menu;
}
chars_task_t;

wuss_window_fn_t chars_handle;

/* create the glyph-grid window against the given wuss instance; does
 * nothing and returns result_OK if wuss has no system font. the font picker
 * loads bmfonts from wuss_get_resources(wuss)/resources/bmfonts/<family>/<style>.png.
 * if out is non-NULL, the task block is also returned through it -- but only
 * on the path where one is actually allocated; *out is left untouched on the
 * font-less early return, same as on any other non-result_OK path */
result_t chars_create(wuss_t *wuss, chars_task_t **out);

/* free a task block allocated by chars_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void chars_destroy(chars_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CHARS_H */
