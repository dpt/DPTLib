/* wuss/test/tasks/launcher.h -- clickable list of names that spawn other tasks */

#ifndef TASKS_LAUNCHER_H
#define TASKS_LAUNCHER_H

#ifdef USE_SDL

#include <stdbool.h>

#include "wuss/icon.h"
#include "wuss/window.h"

typedef result_t (*launcher_spawn_fn_t)(void);

/* one clickable row */
typedef struct launcher_entry
{
  const char         *name;
  launcher_spawn_fn_t spawn;
}
launcher_entry_t;

/* each row is a work-area button icon; clicking one spawns a fresh instance
 * of its task, so a row may be clicked any number of times */
#define LAUNCHER_MAX_ENTRIES 32

typedef struct launcher_task
{
  const launcher_entry_t *entries;  /* owned by the caller, must outlive the window */
  int                     nentries;
  wuss_icon_t            *icons[LAUNCHER_MAX_ENTRIES];
  wuss_window_t          *window;
}
launcher_task_t;

wuss_event_fn_t launcher_handle;

/* create a window of button icons, one per entry; each click on a button
 * calls its spawn function */
result_t launcher_create(wuss_t                 *wuss,
                         const launcher_entry_t *entries,
                         int                     nentries,
                         launcher_task_t        *task);

void launcher_destroy(launcher_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_LAUNCHER_H */
