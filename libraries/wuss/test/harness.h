/* wuss/test/harness.h -- shared fixtures for the wuss tests */

#ifndef WUSS_TEST_HARNESS_H
#define WUSS_TEST_HARNESS_H

#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/menu.h"

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

#endif /* WUSS_TEST_HARNESS_H */
