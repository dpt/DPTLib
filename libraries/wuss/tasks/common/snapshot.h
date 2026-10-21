/* wuss/tasks/common/snapshot.h -- save a task window's content as a PNG */

#ifndef TASKS_SNAPSHOT_H
#define TASKS_SNAPSHOT_H

#ifdef WUSS_APP

#include "base/result.h"
#include "wuss/component/saveas.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* redraw window's whole content box offscreen, by sending handle a
 * wuss_EVENT_REDRAW aimed at a fresh 32bpp bitmap, and write the result to
 * filename. the window's scroll offset is kept, so the file matches what is
 * on screen. does nothing if window is NULL */
result_t snapshot_save_png(wuss_window_t    *window,
                           wuss_window_fn_t *handle,
                           void             *task_data,
                           const char       *filename);

/* create a Save As dialogue for PNG files: wuss_saveas_create with the
 * filetype filled in. save_fn is normally a thin wrapper round
 * snapshot_save_png */
result_t snapshot_saveas_create(wuss_saveas_t        **out,
                                wuss_t                *wuss,
                                const char            *leafname,
                                wuss_saveas_save_fn_t *save_fn,
                                void                  *opaque);

#endif /* WUSS_APP */

#endif /* TASKS_SNAPSHOT_H */
