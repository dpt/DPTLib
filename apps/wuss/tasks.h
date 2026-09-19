/* wuss/tasks.h -- Wuss demo task launcher and menu wiring */

#ifndef WUSS_APP_TASKS_H
#define WUSS_APP_TASKS_H

#include <stdbool.h>

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "geom/point.h"
#include "geom/size.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

#include "frontend.h"

/* The launcher's spawn callbacks take no arguments, so the pieces they need
 * are stashed in this file-scope struct instead; run_wuss runs at most once
 * per process, so it is as good as a passed-around context. run_wuss fills
 * the fields before the event loop; tasks_* and the spawn_* callbacks read
 * them. */
extern struct wuss_app_tasks
{
  wuss_t          *wuss;
  wuss_task_t     *menu_task; /* owns the task launcher menus; also the home
                               * task for the shared proginfo singleton, see
                               * tasks_open_launcher's "Info" row */
  bool             quit; /* set by the "Quit Wuss" task-menu entry */
  wuss_frontend_t *frontend; /* pushed to on wuss_EVENT_PALETTE; see
                              * task_handle_event */
  bitmap_t        *bm;       /* framebuffer bitmap, likewise */
  bool             swap_mouse_buttons; /* set by the Configure task's option
                                        * icon; read by each frontend's raw
                                        * button translator
                                        * (sdl_button_to_wuss,
                                        * mouse_buttons_to_wuss) */
  bool             reverse_scroll; /* set by the Configure task's option
                                    * icon; read where each frontend fills
                                    * wuss_input_event_t.wheel */
}
g_tasks;

/* Change the desktop resolution to width x height: reallocates the
 * framebuffer bitmap and the frontend's backing surface, updates
 * g_tasks.bm/frontend and wuss's own screen_t, then walks every window
 * (via wuss_resize) so none is left off-screen or larger than the new
 * screen. The whole screen is invalidated; the caller's next frame repaints
 * it. Used by the Display task's resolution picker.
 *
 * Returns result_NOT_SUPPORTED on a frontend with a fixed screen mode
 * (RISC OS), leaving everything unchanged; another non-OK result on
 * allocation failure, also leaving everything unchanged. */
result_t app_resize(size2d_t size);

/* the menu-task event handler: dispatches every wuss_EVENT_MENU_SELECT and
 * relays wuss_EVENT_PALETTE to the frontend. Passed as wuss_task_desc.handle
 * when run_wuss creates g.menu_task. */
result_t task_handle_event(wuss_window_t      *window,
                           const wuss_event_t *event,
                           void               *task_data);

/* open the top-level task launcher (Launch / Quit Wuss) at pos; called on a
 * MENU click over bare backdrop */
result_t tasks_open_launcher(point_t pos);

/* Fill out[0..nout-1]: ui[0..nui-1] copied in, then as much of the web-safe
 * 216 (6x6x6 cube, steps of 0x33) as fits, then black for whatever is left.
 * Used to build a paletted screen's full-size palette (nout = whatever
 * bitmap_set_palette will read for the screen's format) from wuss's
 * fixed-size UI palette, both at startup and on a live wuss_EVENT_PALETTE. */
void tasks_build_screen_palette(colour_t       *out,
                                int             nout,
                                const colour_t *ui,
                                int             nui);

#endif /* WUSS_APP_TASKS_H */
