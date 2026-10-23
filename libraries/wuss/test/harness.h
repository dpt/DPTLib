/* wuss/test/harness.h -- shared fixtures for the wuss tests */

#ifndef WUSS_TEST_HARNESS_H
#define WUSS_TEST_HARNESS_H

#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/menu.h"
#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

typedef struct test_task
{
  int                 redraw_count;
  int                 mouse_count;
  wuss_mouse_action_t last_action;
  int                 last_x, last_y;
  wuss_button_t       last_button;
  int                 last_scroll_x, last_scroll_y;
  int                 close_count;
  int                 pre_close_count;
  int                 close_adjust_count;
  int                 close_adjust_mods; /* wuss_key_modifiers_t */
  int                 show_count;
  int                 pre_show_count;
  int                 stop_count;
  int                 open_count;
  int                 palette_count;
  int                 idle_count;
  int                 veto_pre_close;
  int                 veto_pre_show;
  int                 open_window_via_handle;
  const wuss_menu_t  *submenu_to_open;
  int                 pre_submenu_open_count;
  int                 menu_select_count;
  int                 last_menu_index;
  int                 gain_focus_count;
  int                 lose_focus_count;
  int                 key_count;
  int                 last_key;
  int                 last_key_mods;
  int                 unclaim_keys;
  int                 icon_count;
  int                 drag_end_count;
  wuss_window_t      *last_drag_end_drop;
  int                 last_drag_end_x, last_drag_end_y;
  int                 last_drag_end_cancelled;
}
test_task_t;

/* Window handler that tallies events into the test_task_t task_data. */
extern result_t test_handle(wuss_window_t      *window,
                            const wuss_event_t *event,
                            void               *task_data);

/* The registry behind mk_task; a few tests drop entries by hand. */
#define MK_TASK_MAX 64
extern wuss_task_t *mk_task_reg[MK_TASK_MAX];
extern int          mk_task_count;

/* Register a task in one call. Returns NULL on OOM; callers goto Failure. */
extern wuss_task_t *mk_task(wuss_t           *wuss,
                            wuss_window_fn_t *handle,
                            void             *task_data);

/* Destroy every task mk_task created and forget them. */
extern void reap_test_tasks(void);

/* Drop the registry without destroying anything. */
extern void forget_test_tasks(void);

/* Redraw handler that paints one-pixel rows whose colour encodes the
 * document-space Y, so a test can read the framebuffer back and check a blit
 * slid pixels by the right amount. task_data is non-NULL to select that
 * mode; NULL floods the content with a single fixed colour instead. */
extern result_t paint_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data);

/* Redraw handler that floods its whole content box with the colour_t
 * task_data points to, ignoring the clip so wuss-core must clip it. */
extern result_t flood_full_bounds_handle(wuss_window_t      *window,
                                         const wuss_event_t *event,
                                         void               *task_data);

#ifdef WUSS_MENUS
struct wuss__menu;

/* Move the pointer over row `row` of menu level `level`: to the text column
 * (`over_arrow` == 0) or into the submenu-arrow gutter at the row's right
 * edge (`over_arrow` == 1). Used to drive submenu open/close from the tests. */
extern void menu_move_over_row(wuss_t            *wuss,
                               struct wuss__menu *level,
                               int                row,
                               int                over_arrow);
#endif

/* A bitmap-backed screen with a default wuss on it, for tests that each
 * want a fresh instance. */
typedef struct wuss_fixture
{
  void     *pixels;
  bitmap_t  bm;
  screen_t  scr;
  wuss_t   *wuss;
}
wuss_fixture_t;

/* Make a width x height bgrx8888 screen and a default wuss on it, and forget
 * any tasks left over from an earlier test. */
extern result_t wuss_fixture_open(wuss_fixture_t *fixture,
                                  int             width,
                                  int             height);

/* Destroy the wuss (sweeping its tasks) and free the screen. Safe after a
 * failed open. */
extern void wuss_fixture_close(wuss_fixture_t *fixture);

#endif /* WUSS_TEST_HARNESS_H */
