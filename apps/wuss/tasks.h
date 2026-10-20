/* wuss/tasks.h -- Wuss demo task launcher and menu wiring */

#ifndef WUSS_APP_TASKS_H
#define WUSS_APP_TASKS_H

#include <stdbool.h>

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "geom/point.h"
#include "geom/size.h"
#include "io/filetype.h"
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
                               * tasks_open_menu's "Info" row */
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
  bool             crt; /* CRT effect on: requested by --crt, then kept
                         * current by tasks_set_crt */
  bool             pointer; /* software pointer on: requested by --pointer,
                             * then kept current by tasks_set_pointer */
  bool             grey; /* 8bpp screen holds a 256-grey ramp, not colours;
                          * set by app_set_mode(.., app_DEPTH_GREY256) */
  bool             debug_redraw_all;   /* set by the launcher's Debug picks */
  bool             debug_garbage;      /* (having already called */
  bool             debug_pixel_stress; /* wuss_redraw for Redraw); read and
                                        * cleared once per frame by
                                        * wuss_frame */
}
g_tasks;

/* Pseudo depth for app_set_mode / app_get_depth: an 8bpp screen with a
 * 256-level grey ramp palette instead of colours */
#define app_DEPTH_GREY256 (-8)

/* Change the desktop resolution to width x height and the framebuffer to
 * depth bits per pixel (1, 2, 4, 8, 15, 16, 32 or app_DEPTH_GREY256): reallocates the framebuffer
 * bitmap and the frontend's backing surface, updates g_tasks.bm/frontend and
 * wuss's own screen_t, then walks every window (via wuss_resize) so none is
 * left off-screen or larger than the new screen. A depth change also rebuilds
 * the screen palette. The whole screen is
 * invalidated; the caller's next frame repaints it. Used by the Display
 * task's pickers.
 *
 * Returns result_NOT_SUPPORTED on a frontend with a fixed screen mode
 * (RISC OS) and result_BAD_ARG for a depth the frontend can't do, leaving
 * everything unchanged; another non-OK result on allocation failure, also
 * leaving everything unchanged. */
result_t app_set_mode(size2d_t size, int depth);

/* the framebuffer's current bits per pixel */
int app_get_depth(void);

/* Turn the software pointer on or off, hiding or showing the OS pointer to
 * match. Returns whether it is now on: false if the "pointer" icon pool entry
 * failed to load or the frontend keeps its own pointer (RISC OS). */
bool app_set_pointer(bool on);

/* the menu-task event handler: dispatches every wuss_EVENT_MENU_SELECT and
 * relays wuss_EVENT_PALETTE to the frontend. Passed as wuss_task_desc.handle
 * when run_wuss creates g.menu_task. */
result_t task_handle_event(wuss_window_t      *window,
                           const wuss_event_t *event,
                           void               *task_data);

#ifdef WUSS_ICONBAR
/* open the Wuss menu (Info / Zoom / CRT / Pointer / Debug / Quit Wuss) above
 * the icon bar; called on a MENU click on the TaskManager icon */
result_t tasks_open_menu(void);
#endif

/* offer a key the focused window passed on to the task launcher's menu
 * shortcuts (e.g. Ctrl-Shift-Q for Quit Wuss); result_WUSS_KEY_UNCLAIMED if none
 * matches */
result_t tasks_launcher_key(int code, wuss_key_modifiers_t modifiers);

/* System key chords (Ctrl-F2 close window, Ctrl-Q quit app, Ctrl-Shift-Q quit
 * Wuss), offered before the focused window so no task can swallow them.
 * Returns non-zero if the key was used. */
int tasks_system_key(int code, wuss_key_modifiers_t modifiers);

/* Destroy every app whose Quit was picked, with all its instances. Call once
 * per frame, after wuss_idle. */
void tasks_reap_apps(void);

/* Switch the frontend's CRT post-effect on or off, recording the outcome in
 * g_tasks.crt. Needs g_tasks.frontend set. */
void tasks_set_crt(bool on);

/* Switch the software pointer on or off (app_set_pointer), recording the
 * outcome in g_tasks.pointer. */
void tasks_set_pointer(bool on);

/* Install the apps named in names, a comma-separated list of launcher-menu
 * task names (e.g. "Saturn,Clock"), or "all" to install every task in every
 * category (games, image, tests, toys, system, text, utilities, visuals), in table
 * order; an empty string installs nothing. Installing puts the app's icon on
 * the icon bar but opens no window: that waits for a SELECT click on the icon.
 * Logs and continues past any individual failure or unknown name rather
 * than aborting the rest. Called once at startup, after g_tasks.wuss is
 * set. */
void tasks_spawn(const char *names);

/* Install the app that opens files of type, as tasks_spawn does: Filing's
 * run function (see filing_set_run_fn). Returns result_NOT_FOUND if no app
 * opens that type. */
result_t tasks_run_type(filetype_t type);

/* The launchable tasks, grouped into categories (the folders AppsFiler
 * shows). Indices are valid below the matching count; names are static. */
int tasks_category_count(void);
const char *tasks_category_name(int category);
int tasks_entry_count(int category);
const char *tasks_entry_name(int category, int index);

/* Install task index of category, as tasks_spawn does, logging any
 * failure. Does nothing if it is already installed. */
result_t tasks_entry_spawn(int category, int index);

/* Fill out[0..nout-1]: ui[0..nui-1] copied in, then as much of the web-safe
 * 216 (6x6x6 cube, steps of 0x33) as fits, then black for whatever is left.
 * Used to build a paletted screen's full-size palette (nout = whatever
 * bitmap_set_palette will read for the screen's format) from wuss's
 * fixed-size UI palette, both at startup and on a live wuss_EVENT_PALETTE.
 * nout 2 (1bpp) or 4 (2bpp), or 256 when g_tasks.grey is set, instead gets
 * a black-to-white grey ramp. */
void tasks_build_screen_palette(colour_t       *out,
                                int             nout,
                                const colour_t *ui,
                                int             nui);

#endif /* WUSS_APP_TASKS_H */
