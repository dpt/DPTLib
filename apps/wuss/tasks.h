/* wuss/tasks.h -- Wuss demo task launcher and menu wiring */

#ifndef WUSS_APP_TASKS_H
#define WUSS_APP_TASKS_H

#include <stdbool.h>

#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "geom/point.h"
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
  wuss_task_t     *menu_task; /* owns the task launcher menus */
  colour_t        *palette;
  int              npalette;
  const char      *palette_name; /* startup *.hex leafname, for ticking the
                                  * picker menu's initial selection */
  const char      *resources;
  bmfont_t        *daydream_font;
  bmfont_t        *bold_font;
  bool             quit; /* set by the "Quit Wuss" task-menu entry */
  wuss_frontend_t *frontend; /* pushed to on wuss_EVENT_PALETTE; see
                              * task_handle_event */
  bitmap_t        *bm;       /* framebuffer bitmap, likewise */
}
g;

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
