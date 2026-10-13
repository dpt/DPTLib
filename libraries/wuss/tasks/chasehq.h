/* wuss/tasks/chasehq.h -- Chase H.Q. task */

#ifndef TASKS_CHASEHQ_H
#define TASKS_CHASEHQ_H

#if defined(WUSS_APP) && defined(WUSS_CHASEHQ)

#include "wuss/task.h"
#include "wuss/wuss.h"

typedef struct chasehq_task chasehq_task_t;

/* create a window running the ZX Spectrum Chase H.Q. port (built from
 * WUSS_CHASEHQ_DIR) on its own thread. Keys are the game's own (K/L steer,
 * A/Z accelerate/brake, N gear, Space turbo); the arrow keys and '.' drive
 * the emulated Kempston joystick. Silent. if out is non-NULL, the task block
 * is also returned through it */
result_t chasehq_create(wuss_t *wuss, chasehq_task_t **out);

/* install the icon bar app that opens chasehq windows on demand */
result_t chasehq_app_create(wuss_t *wuss);

#endif /* WUSS_APP && WUSS_CHASEHQ */

#endif /* TASKS_CHASEHQ_H */
