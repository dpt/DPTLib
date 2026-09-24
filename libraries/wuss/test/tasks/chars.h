/* wuss/test/tasks/chars.h -- system font glyph grid task */

#ifndef TASKS_CHARS_H
#define TASKS_CHARS_H

#ifdef WUSS_APP

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/fontmenu.h"
#include "wuss/component/proginfo.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* displays every glyph (0-255) of a bitmap font as a 32x8 grid, so the whole
 * font can be eyeballed at a glance. A MENU click on the window opens a
 * picker -- the shared wuss_fontmenu singleton over resources/bmfonts --
 * that swaps the font in place. */
typedef struct chars_task
{
  wuss_window_t      *window;
  wuss_task_t        *delegate;    /* the wuss task backing this window */
  wuss_t             *wuss;        /* for wuss_get_pointer when opening the
                                    * menu */
  wuss_menu_handle_t  menu_handle; /* the open chain, to re-tick it live on
                                    * an ADJUST pick that keeps it open */
  bmfont_t           *font;        /* currently shown; == fonts[current] */
  bmfont_t          **fonts;       /* one slot per menu item, lazily loaded */
  int                 nfonts;      /* length of fonts[]; == menu item count */
  int                 current;     /* index into fonts[], or -1 for sysfont */
  colour_t            fg, mg, bg;
  wuss_menu_item_t    menu_items[2]; /* per-instance: a shared static would
                                      * leak one instance's .window pointer
                                      * into another's menu */
  wuss_menu_t         menu;
}
chars_task_t;

wuss_window_fn_t chars_handle;

/* create the glyph-grid window against the given wuss instance; does
 * nothing and returns result_OK if wuss has no system font. the font picker
 * loads bmfonts from wuss_get_resources(wuss)/resources/bmfonts/<name>.png.
 * if out is non-NULL, the task block is also returned through it -- but only
 * on the path where one is actually allocated; *out is left untouched on the
 * font-less early return, same as on any other non-result_OK path */
result_t chars_create(wuss_t *wuss, chars_task_t **out);

/* free a task block allocated by chars_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void chars_destroy(chars_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_CHARS_H */
