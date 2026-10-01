/* wuss/test/tasks/keys.h -- key input demo task */

#ifndef TASKS_KEYS_H
#define TASKS_KEYS_H

#ifdef WUSS_APP

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/component/proginfo.h"
#include "wuss/menu.h"
#include "wuss/window.h"

#define KEYS_HISTORY 6 /* most recent keys shown, newest first */

/* a focusable window that shows whether it holds the input focus and the
 * last few keys it was sent, each with its modifiers. A Select click gives
 * it the focus; Menu > Clear empties the history. */
typedef struct keys_task
{
  wuss_t              *wuss; /* for wuss_get_focus and wuss_get_pointer */
  wuss_task_t         *delegate; /* the task that owns the menu */
  wuss_window_t       *window;
  wuss_menu_handle_t   menu_handle; /* live only between open and a pick */
  wuss_menu_item_t     menu_items[2]; /* per-instance: a shared static would
                                        * leak one instance's .window
                                        * pointer into another's menu */
  wuss_menu_t          menu;
  bmfont_t            *font; /* borrowed */
  colour_t             bg, fg;
  int                  keys[KEYS_HISTORY]; /* newest first */
  wuss_key_modifiers_t mods[KEYS_HISTORY]; /* keys[i]'s modifiers */
  int                  nkeys; /* used entries of keys[] */
}
keys_task_t;

wuss_window_fn_t keys_handle;

/* create the keys window against the given wuss instance. if out is
 * non-NULL, the task block is also returned through it */
result_t keys_create(wuss_t *wuss, keys_task_t **out);

/* free a task block allocated by keys_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void keys_destroy(keys_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_KEYS_H */
