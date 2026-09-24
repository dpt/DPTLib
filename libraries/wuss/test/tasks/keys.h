/* wuss/test/tasks/keys.h -- key input demo task */

#ifndef TASKS_KEYS_H
#define TASKS_KEYS_H

#ifdef WUSS_APP

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/window.h"

/* a focusable window that shows the last key it was sent, its modifiers and
 * whether it holds the input focus. A Select click gives it the focus. */
typedef struct keys_task
{
  wuss_t              *wuss; /* for wuss_get_focus */
  wuss_window_t       *window;
  bmfont_t            *font; /* borrowed */
  colour_t             bg, fg;
  int                  key;  /* last key code, or -1 before the first */
  wuss_key_modifiers_t mods;
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
