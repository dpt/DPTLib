/* launcher.h -- wuss test - clickable list of names that spawn other tasks */

#ifndef TASKS_LAUNCHER_H
#define TASKS_LAUNCHER_H

#ifdef USE_SDL

#include <stdbool.h>

#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "wuss/window.h"

typedef result_t (*launcher_spawn_fn_t)(void);

/* one clickable row; "running" is set once spawned and never cleared, so a
 * second click is a no-op -- relaunching a task after its window closes
 * needs the test restarted */
typedef struct launcher_entry
{
  const char          *name;
  launcher_spawn_fn_t  spawn;
  bool                 running;
}
launcher_entry_t;

typedef struct launcher_task
{
  launcher_entry_t *entries;  /* owned by the caller, must outlive the window */
  int               nentries;
  bmfont_t         *font;
  colour_t          fg, bg, running_fg;
  wuss_window_t    *window;
}
launcher_task_t;

wuss_event_fn_t launcher_handle;

/* create a window listing "entries"; clicking a row calls its spawn
 * function once */
result_t launcher_create(wuss_t           *wuss,
                         launcher_entry_t *entries,
                         int               nentries,
                         bmfont_t         *font,
                         const colour_t   *palette,
                         launcher_task_t  *task);

void launcher_destroy(launcher_task_t *task);

#endif /* USE_SDL */

#endif /* TASKS_LAUNCHER_H */
