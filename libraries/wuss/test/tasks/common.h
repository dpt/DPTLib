/* wuss/test/tasks/common.h -- helpers shared by the wuss demo tasks */

#ifndef TASKS_COMMON_H
#define TASKS_COMMON_H

#include "base/result.h"
#include "geom/size.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* braced initialiser for a wuss_proginfo_desc_t, filling in the author and
 * version lines every demo task shares */
#define TASK_PROGINFO_DESC(name, purpose) \
  { name, purpose, "© DPTLib contributors", "1.0 (" __DATE__ ")" }

/* create a focusable window with no backdrop whose document is exactly size,
 * as most demo tasks want */
static inline result_t task_window_create(wuss_task_t    *task,
                                          size2d_t        size,
                                          const char     *title,
                                          wuss_window_t **window)
{
  return wuss_window_create_placed(task,
                                   size,
                                   title,
                                   wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                                   wuss_NO_BACKDROP,
                                   size,
                                   SIZE2D(0, 0),
                                   window);
}

#endif /* TASKS_COMMON_H */
