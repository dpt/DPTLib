/* wuss/core/windows.h -- wuss - window stacking and visibility */

#ifndef WUSS_CORE_WINDOWS_H
#define WUSS_CORE_WINDOWS_H

#include "types.h"


/* Walk every window front to back across all three stacks: wuss__z_first is
 * the frontmost window overall (NULL if none), wuss__z_below the next one
 * behind "window" (NULL at the very back). */
wuss_window_t *wuss__z_first(wuss_t *wuss);

/* Tell every ordinary window's task that the room its layout has changed --
 * the screen size or the furniture metrics -- by delivering
 * wuss_EVENT_RELAYOUT to each. Called last by wuss_resize and
 * wuss_set_config, so the windows have already been moved and shrunk. A
 * non-OK result from a handler is returned (the first one), but delivery
 * continues past it. */
result_t wuss__relayout_all(wuss_t *wuss);
wuss_window_t *wuss__z_below(const wuss_window_t *window);

/* Recover the owning wuss_window_t from a node in a task's window list
 * (window->task_link). task_link is not the first member, so a plain cast
 * won't do. */
#define wuss__window_from_task_link(node) \
  ((wuss_window_t *) ((char *) (node) - offsetof(struct wuss_window, task_link)))

/* Recover the owning wuss_window_t from a node in the z-order chain
 * (window->link). link is the first member, so this is just a cast -- but
 * naming it keeps every z-order walk honest about depending on that. */
#define wuss__window_from_link(node) \
  ((wuss_window_t *) (void *) (node))

/* Recover the owning wuss_task_t from a node in the task chain (task->link).
 * link is the first member, so this is just a cast -- but naming it keeps
 * every task walk honest about depending on that. */
#define wuss__task_from_link(node) \
  ((wuss_task_t *) (void *) (node))

/* wuss_window_set_hidden's real body. `handle`/`index` (menu builds only)
 * are threaded through into wuss_EVENT_PRE_SHOW's payload so a flagged menu
 * leaf's recipient can call wuss_menu_open_window_now(handle, index) to opt
 * in to the reveal. NULL/-1 (a plain window, or an unflagged menu leaf --
 * both reach this same path, PRE_SHOW always fires) makes the reveal
 * proceed by default. Defined in window/set-hidden.c. */
#ifdef WUSS_MENUS
result_t wuss__window_set_hidden_ex(wuss_window_t     *window,
                                    int                hidden,
                                    struct wuss__menu *handle,
                                    int                index);
#else
result_t wuss__window_set_hidden_ex(wuss_window_t *window, int hidden);
#endif

#endif /* WUSS_CORE_WINDOWS_H */
