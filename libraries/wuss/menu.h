/* wuss/menu.h -- wuss pop-up menu helper, internal */

#ifndef WUSS_MENU_IMPL_H
#define WUSS_MENU_IMPL_H

#include "base/utils.h"
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
 * link to the level it opened from. The head (root) is the session chain
 * (wuss__menu_chain); each child points at its parent so wuss_menu_close can
 * walk leaf-to-root. */
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

  /* stack a borrowed-window level's window was in before the chain moved it
   * to wuss_STACK_TOP; restored when the level closes. Unused otherwise. */
  wuss_stack_t           saved_stack;

  /* window that held the input focus before a borrowed-window level took it
   * for its first writable; refocused when the level closes, unless it has
   * since gone or something else took the focus. NULL otherwise. */
  wuss_window_t         *saved_focus;

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

#ifdef WUSS_MENUS

/* ----- menu session entry points (core calls only these) ----- */

/* Allocate / free the per-wuss_t menu session (wuss->menu). Create runs in
 * wuss_create, free as the last step of wuss_destroy: window closes during the
 * z_order sweep still consult the session. */
result_t wuss__menu_session_create(wuss_t *wuss);
void     wuss__menu_session_free(wuss_t *wuss);

/* wuss_destroy: close any chain a client QUIT left open, then QUIT and free
 * the internal menu task. Run after every client task is gone. */
void wuss__menu_teardown(wuss_t *wuss);

/* True if `task` is the internal task owning the menu windows; wuss_destroy's
 * client sweep skips it so it outlives every client QUIT. */
int wuss__menu_is_task(const wuss_t *wuss, const wuss_task_t *task);

/* Head of the open chain, NULL when none. */
struct wuss__menu *wuss__menu_chain(const wuss_t *wuss);

/* Called from wuss_mouse_click for every press/release before the window
 * hit-test. Returns 1 if the event is spent -- the tail of the press that
 * opened the chain, or a MOUSE_DOWN outside every chain window (which closes
 * the chain, unless it is a MENU press landing on another window) -- else 0. */
int wuss__menu_press(wuss_t              *wuss,
                     wuss_mouse_action_t  action,
                     wuss_button_t        button,
                     const wuss_window_t *hit);

/* Called from wuss_key_press: Escape dismisses an open chain. Returns 1 if the
 * key was claimed. */
int wuss__menu_key(wuss_t *wuss, int code);

/* wuss_window_close: `doomed` is about to go. Forgets it as a focus lender and
 * abandons the chain if it is one of the chain's windows. */
void wuss__menu_window_closing(wuss_t *wuss, wuss_window_t *doomed);

/* A task is about to get its QUIT: abandon the chain if the task owns it. */
void wuss__menu_task_closing(wuss_t *wuss, const wuss_task_t *task);

/* True if `icon` is a parent row on an open menu chain whose submenu is open --
 * i.e. some chain level's `open_index` row. Such a row keeps its hover
 * highlight while the pointer is in a deeper level; wuss__icon_set_hover asks
 * before dropping a row's highlight. */
int wuss__menu_row_pinned(const wuss_t *wuss, const wuss_icon_t *icon);

#else /* !WUSS_MENUS */

static inline result_t wuss__menu_session_create(wuss_t *wuss)
{
  NOT_USED(wuss);
  return result_OK;
}

static inline void wuss__menu_session_free(wuss_t *wuss)
{
  NOT_USED(wuss);
}

static inline void wuss__menu_teardown(wuss_t *wuss)
{
  NOT_USED(wuss);
}

static inline int wuss__menu_is_task(const wuss_t      *wuss,
                                     const wuss_task_t *task)
{
  NOT_USED(wuss);
  NOT_USED(task);
  return 0;
}

static inline int wuss__menu_press(wuss_t              *wuss,
                                   wuss_mouse_action_t  action,
                                   wuss_button_t        button,
                                   const wuss_window_t *hit)
{
  NOT_USED(wuss);
  NOT_USED(action);
  NOT_USED(button);
  NOT_USED(hit);
  return 0;
}

static inline int wuss__menu_key(wuss_t *wuss, int code)
{
  NOT_USED(wuss);
  NOT_USED(code);
  return 0;
}

static inline void wuss__menu_window_closing(wuss_t        *wuss,
                                             wuss_window_t *doomed)
{
  NOT_USED(wuss);
  NOT_USED(doomed);
}

static inline void wuss__menu_task_closing(wuss_t            *wuss,
                                           const wuss_task_t *task)
{
  NOT_USED(wuss);
  NOT_USED(task);
}

#endif /* WUSS_MENUS */

#endif /* WUSS_MENU_IMPL_H */
