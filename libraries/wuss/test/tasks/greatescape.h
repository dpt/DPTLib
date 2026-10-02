/* wuss/test/tasks/greatescape.h -- The Great Escape task */

#ifndef TASKS_GREATESCAPE_H
#define TASKS_GREATESCAPE_H

#if defined(WUSS_APP) && defined(WUSS_GREATESCAPE)

#include "wuss/task.h"
#include "wuss/wuss.h"

typedef struct greatescape_task greatescape_task_t;

wuss_window_fn_t greatescape_handle;

/* create a window running the ZX Spectrum The Great Escape recreation (built
 * from WUSS_GREATESCAPE_DIR) on its own thread. Keys are the game's own,
 * chosen from its menu; the arrow keys and '.' drive the emulated Kempston
 * joystick. Silent. if out is non-NULL, the task block is also returned
 * through it */
result_t greatescape_create(wuss_t *wuss, greatescape_task_t **out);

/* stop the game thread and free a task block allocated by greatescape_create;
 * normally called by the window's wuss_EVENT_QUIT handler, not by callers
 * directly */
void greatescape_destroy(greatescape_task_t *task);

#endif /* WUSS_APP && WUSS_GREATESCAPE */

#endif /* TASKS_GREATESCAPE_H */
