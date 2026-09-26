/* wuss/test/tasks/snapshot.h -- save a task window's content as a PNG */

#ifndef TASKS_SNAPSHOT_H
#define TASKS_SNAPSHOT_H

#ifdef WUSS_APP

#include "base/result.h"
#include "wuss/window.h"

/* redraw window's whole content box offscreen, by sending handle a
 * wuss_EVENT_REDRAW aimed at a fresh 32bpp bitmap, and write the result to
 * filename. the window's scroll offset is kept, so the file matches what is
 * on screen. does nothing if window is NULL */
result_t snapshot_save_png(wuss_window_t    *window,
                           wuss_window_fn_t *handle,
                           void             *task_data,
                           const char       *filename);

#endif /* WUSS_APP */

#endif /* TASKS_SNAPSHOT_H */
