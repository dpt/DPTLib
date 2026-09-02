/* wuss/menu.h -- wuss pop-up menu helper, internal */

#ifndef WUSS_MENU_IMPL_H
#define WUSS_MENU_IMPL_H

#include "geom/point.h"

#include "wuss/wuss.h"
#include "wuss/menu.h"

/* One open level of a menu chain: its window and per-item icon handles, plus a
 * link to the level it opened from. The head (root) is wuss->menu_chain; each
 * child points at its parent so wuss_menu_close can walk leaf-to-root. */
struct wuss__menu
{
  wuss_t                *wuss;
  wuss_task_t           *owner;    /* task that opened the chain (from
                                    * wuss_menu_open); MENU_SELECT is
                                    * delivered here. Only meaningful on the
                                    * root; children copy it for convenience */
  wuss_window_t         *window;   /* the borderless menu window; for a
                                    * borrowed-window level (see borrowed) the
                                    * caller's own window instead */
  const wuss_menu_t     *menu;     /* borrowed source description; NULL for a
                                    * borrowed-window level */
  wuss_icon_t          **icons;    /* owned array, menu->nitems entries, in
                                    * item order; NULL for a borrowed-window
                                    * level */
  struct wuss__menu     *parent;   /* level this one opened from, NULL at root */
  struct wuss__menu     *child;    /* open submenu level, NULL when none */
  int                    open_index; /* item index whose submenu `child` is,
                                      * -1 when no child open */
  int                    borrowed;   /* 1: `window` is a caller-owned window
                                      * shown in place of a submenu -- hide it
                                      * rather than close it on teardown */
};

/* Called from wuss_mouse_click on a MOUSE_DOWN before the window hit-test: if a
 * menu chain is open and `hit` is not one of its windows, close the whole chain
 * and return 1 (the click is spent on dismissal). Returns 0 otherwise. */
int wuss__menu_click_outside(wuss_t *wuss, const wuss_window_t *hit);

#endif /* WUSS_MENU_IMPL_H */
