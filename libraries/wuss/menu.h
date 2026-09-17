/* wuss/menu.h -- wuss pop-up menu helper, internal */

#ifndef WUSS_MENU_IMPL_H
#define WUSS_MENU_IMPL_H

#include "geom/point.h"

#include "wuss/wuss.h"
#include "wuss/menu.h"

/* Internal wuss__menu::flags bits. */
enum
{
  /* `window` is a caller-owned window shown in place of a submenu -- hide it
   * rather than close it on teardown */
  wuss_MENU__BORROWED         = 1u << 0,

  /* an ADJUST pick's flash in progress (see flash below): do not tear the
   * chain down before MENU_SELECT */
  wuss_MENU__FLASH_KEEP_OPEN  = 1u << 1
};

/* One open level of a menu chain: its window and per-item icon handles, plus a
 * link to the level it opened from. The head (root) is wuss->menu_chain; each
 * child points at its parent so wuss_menu_close can walk leaf-to-root. */
struct wuss__menu
{
  unsigned int           flags;

  wuss_t                *wuss;

  /* task that opened the chain (from wuss_menu_open); MENU_SELECT is
   * delivered here. Only meaningful on the root; children copy it for
   * convenience */
  wuss_task_t           *owner;

  /* the borderless menu window; for a borrowed-window level (see
   * wuss_MENU__BORROWED) the caller's own window instead */
  wuss_window_t         *window;

  /* borrowed source description; NULL for a borrowed-window level */
  const wuss_menu_t     *menu;

  /* owned array, menu->nitems entries, in item order; NULL for a
   * borrowed-window level */
  wuss_icon_t          **icons;

  /* level this one opened from, NULL at root */
  struct wuss__menu     *parent;

  /* open submenu level, NULL when none */
  struct wuss__menu     *child;

  /* item index whose submenu `child` is, -1 when no child open */
  int                    open_index;

  /* item index whose PRE_SHOW / PRE_SUBMENU_OPEN is being delivered on this
   * level right now, else -1. Set immediately before wuss__deliver,
   * consumed by wuss_menu_open_window_now / wuss_menu_open_submenu_now if
   * the handler calls back synchronously, cleared once delivery returns. */
  int                    pending_index;

  /* SELECT-pick flash: the picked row's highlight is toggled a few times off
   * wuss_EVENT_IDLE before the chain closes and MENU_SELECT is delivered.
   * Set on the picked level; frames == 0 means no flash in progress. The
   * owner/menu/index/button are captured because the chain (and this node)
   * is freed before the deferred notification goes out. */
  struct
  {
    /* IDLE frames left in the flash */
    int                  frames;

    /* item index being flashed */
    int                  index;

    /* captured MENU_SELECT target */
    wuss_task_t         *owner;

    /* captured menu for the notification */
    const wuss_menu_t   *menu;

    /* captured release button */
    wuss_button_t        button;
  }
  flash;
};

/* Called from wuss_mouse_click on a MOUSE_DOWN before the window hit-test: if a
 * menu chain is open and `hit` is not one of its windows, close the whole chain
 * and return 1 (the click is spent on dismissal). Returns 0 otherwise. */
int wuss__menu_click_outside(wuss_t *wuss, const wuss_window_t *hit);

/* True if `icon` is a parent row on an open menu chain whose submenu is open --
 * i.e. some chain level's `open_index` row. Such a row keeps its hover
 * highlight while the pointer is in a deeper level; wuss__icon_set_hover asks
 * before dropping a row's highlight. */
int wuss__menu_row_pinned(const wuss_t *wuss, const wuss_icon_t *icon);

#endif /* WUSS_MENU_IMPL_H */
