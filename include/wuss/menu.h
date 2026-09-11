/* wuss/menu.h -- wuss pop-up menu helper */

/**
 * \file menu.h
 *
 * A thin helper for RISC OS-style pop-up menus: a menu is a borderless
 * window populated with wuss_ICON_TYPE_MENU_ENTRY icons, but wuss owns the
 * plumbing -- layout, placement, submenu chaining on hover and whole-chain
 * dismissal on a click outside or a leaf selection.
 *
 * Menus are described by caller-owned wuss_menu_t / wuss_menu_item_t
 * structures. wuss_menu_open treats them as immutable; a task that wants a
 * tick to change just edits its own array and reopens the menu, or lets
 * wuss_menu_open_ticked set the ticks for it (the one call that writes back
 * to the item array).
 *
 * Built only when WUSS_MENUS is defined (which implies WUSS_ICONS).
 */

#ifndef WUSS_MENU_H
#define WUSS_MENU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/point.h"

#include "wuss/wuss.h"
#include "wuss/window.h"

/* ----------------------------------------------------------------------- */

/** Per-item flags for a wuss_menu_item_t. OR'd together. */
typedef enum wuss_menu_item_flags
{
  wuss_MENU_ITEM_NONE     = 0,

  /** Draw a tick at the item's left edge */
  wuss_MENU_ITEM_TICKED   = 1 << 0,

  /** Draw a dashed rule above this item, marking a group boundary. The
   *  item is otherwise an ordinary row: it keeps its label and responds
   *  to the pointer. The rule is laid out and drawn separately and is not
   *  interactive */
  wuss_MENU_ITEM_DASHED   = 1 << 1,

  /** Greyed, never highlights, not selectable */
  wuss_MENU_ITEM_DISABLED = 1 << 2,

  /** Draw a colour chip of \c swatch at the item's left edge, in place of
   *  a tick */
  wuss_MENU_ITEM_SWATCH   = 1 << 3,

  /** \c submenu is borrowed (e.g. patched in after the menu was built) and
   *  must outlive the tree, not be freed with it: wuss_menu_destroy leaves
   *  it alone instead of recursing into it. Ignored on an item with no
   *  \c submenu. */
  wuss_MENU_ITEM_BORROWED_SUBMENU = 1 << 4
}
wuss_menu_item_flags_t;

/** One row of a menu. */
typedef struct wuss_menu_item
{
  /** Row label; NULL is treated as "" */
  const char             *text;

  /** See wuss_menu_item_flags_t */
  wuss_menu_item_flags_t  flags;

  /** Non-NULL: draw a right arrow and open this menu to the right on
   *  hover */
  const struct wuss_menu *submenu;

  /** Non-NULL: draw a right arrow and, on hover, show this caller-owned
   *  window where a submenu would open, hiding it again when the pointer
   *  leaves the row or the chain is dismissed. Create it with
   *  wuss_WINDOW_HIDDEN. Mutually exclusive with \c submenu. The window
   *  must outlive the open chain -- do not wuss_window_close it while its
   *  menu is open. */
  wuss_window_t          *window;

  /** With wuss_MENU_ITEM_SWATCH: the colour chip to draw at the row's
   *  left edge, as an index into the system palette. Ignored without that
   *  flag, so a zero-initialised item is unaffected. */
  wuss_colour_t           swatch;
}
wuss_menu_item_t;

/** A menu: an array of items the caller owns. */
typedef struct wuss_menu
{
  const char             *title;  /**< titlebar caption; NULL treated as "" */
  const wuss_menu_item_t *items;
  int                     nitems;
}
wuss_menu_t;

/** Opaque handle to an open menu chain. */
typedef struct wuss__menu *wuss_menu_handle_t;

/* ----------------------------------------------------------------------- */

/**
 * Open \p menu as a pop-up at \p at (screen space), nudged to stay on
 * screen. Any menu chain already open is closed first. The chain lives until
 * a leaf is SELECT-picked, a click lands outside every menu window, or
 * wuss_menu_close is called.
 *
 * When a leaf item is released over, a wuss_EVENT_MENU_SELECT event is
 * delivered to \p task's handle (with window == NULL); its data.menu_select
 * carries the (sub)menu, the item index and the release button.
 *
 * If wuss instead closes the chain itself -- a click outside every menu
 * window, or a later wuss_menu_open -- \p task's handle gets a
 * wuss_EVENT_MENU_CLOSED (window == NULL, no data). A task that kept \p out
 * must drop it there; the chain is freed by the time the event arrives. No
 * such event follows a wuss_menu_close the task made.
 *
 * \param[in]  task Task opening the menu; receives wuss_EVENT_MENU_SELECT.
 *                  The menu windows are wuss-owned, not task's.
 * \param[in]  menu Menu to show; borrowed, must outlive the open chain.
 * \param[in]  at   Where to put the menu's top-left, screen space.
 * \param[out] out  Filled with the chain handle, or NULL if not wanted.
 * \return \ref result_OK, \ref result_OOM, or a wuss_window_create code.
 */
result_t wuss_menu_open(wuss_task_t        *task,
                        const wuss_menu_t  *menu,
                        point_t             at,
                        wuss_menu_handle_t *out);

/** Close a menu chain and every window in it. Safe to pass a stale or NULL
 *  handle. */
void wuss_menu_close(wuss_menu_handle_t handle);

/** Non-zero while \p handle refers to a currently open chain. */
int wuss_menu_is_open(wuss_menu_handle_t handle);

/**
 * True when \p ev is a wuss_EVENT_MENU_SELECT whose pick keeps the chain
 * open -- an ADJUST-button release (see wuss_menu_open). A SELECT-button
 * pick has already closed and freed the chain by the time the event arrives,
 * so a task that stored the wuss_menu_open handle must drop it in that case:
 *
 * \code if (!wuss_menu_should_keep_open(ev)) task->menu_handle = NULL;
 * \endcode
 *
 * \param[in] ev The event passed to the task's handle callback.
 * \return Non-zero if \p ev is a MENU_SELECT that leaves the chain open.
 */
int wuss_menu_should_keep_open(const wuss_event_t *ev);

/* ----------------------------------------------------------------------- */

/* Setting ticks on menu data, whether or not it is currently open -- a task
 * that edits the item array before a (re)opening wuss_menu_open, rather than
 * re-ticking an already-open chain. See below for the _live equivalents that
 * act on an open chain and redraw the changed rows in place. */

/**
 * Set every row's tick from \p ticked (row i ticked iff bit i is set) on a
 * menu that need not be open. \p menu is mutated (its items'
 * wuss_MENU_ITEM_TICKED bit only); every other flag on each item is left as
 * the caller set it. \c menu->nitems must not exceed the bit width of
 * <tt>unsigned int</tt>.
 *
 * \param[in,out] menu   Menu whose items' ticks are set.
 * \param[in]     ticked Bit i ticks row i; 0 unticks every row.
 */
void wuss_menu_tick_set(wuss_menu_t *menu, unsigned int ticked);

/**
 * Tick \p index and untick every other row of \p menu, whether or not it is
 * open. Data-level equivalent of wuss_menu_tick_exclusive_live, for a task
 * that edits the item array before a fresh wuss_menu_open rather than
 * re-ticking an already-open chain.
 *
 * \param[in,out] menu  Menu whose items' ticks are set.
 * \param[in]     index Row to tick, or -1 to untick every row.
 */
void wuss_menu_tick_exclusive(wuss_menu_t *menu, int index);

/**
 * Set or clear \p index's tick in \p menu, leaving every other item's tick
 * untouched, whether or not the menu is open. Data-level equivalent of
 * wuss_menu_tick_item_live, for a menu that tracks more than one independent
 * tick (see wuss_menu_tick_item_live for why).
 *
 * \param[in,out] menu   Menu whose item's tick is set.
 * \param[in]     index  Row to update. A no-op if out of range.
 * \param[in]     ticked Non-zero to tick the row, zero to untick it.
 */
void wuss_menu_tick_item(wuss_menu_t *menu, int index, int ticked);

/**
 * Set every row's tick from \p ticked (as wuss_menu_tick_set), then open \p
 * menu -- the tick-sync-then-open a task otherwise hand-rolls before each
 * wuss_menu_open.
 *
 * \param[in]  task   As wuss_menu_open.
 * \param[in]  menu   As wuss_menu_open, but non-const: its ticks are set.
 * \param[in]  ticked Bit i ticks row i; 0 unticks every row.
 * \param[in]  at     As wuss_menu_open.
 * \param[out] out    As wuss_menu_open.
 * \return As wuss_menu_open.
 */
result_t wuss_menu_open_ticked(wuss_task_t        *task,
                               wuss_menu_t        *menu,
                               unsigned int        ticked,
                               point_t             at,
                               wuss_menu_handle_t *out);

/* ----------------------------------------------------------------------- */

/* Re-ticking a currently open menu level in place, for a task that keeps an
 * ADJUST-picked menu open (see wuss_menu_open) and wants its own tick to
 * change without rebuilding the chain: an ADJUST pick delivers
 * wuss_EVENT_MENU_SELECT but does not close or redraw the menu, so a task
 * that only edits its wuss_menu_item_t.flags array never sees it take effect
 * on screen. These invalidate the changed rows; the data-level functions
 * above do not. */

/**
 * Ticks item \p index and unticks every other item of the open level whose
 * \c menu is \p menu (searched from \p handle's chain), then invalidates the
 * changed rows. A no-op if \p handle is stale/closed, \p menu is not an open
 * level of its chain, or \p index is out of range (still unticking every row
 * in that case).
 *
 * \param[in] handle Chain handle from wuss_menu_open.
 * \param[in] menu   The (sub)menu level to update; matched by pointer
 *                   against the description passed to wuss_menu_open or
 *                   reached via a wuss_menu_item_t.submenu.
 * \param[in] index  Row to tick, or -1 to untick every row.
 */
void wuss_menu_tick_exclusive_live(wuss_menu_handle_t handle,
                                   const wuss_menu_t *menu,
                                   int                index);

/**
 * Set every row's tick from \p ticked (row i ticked iff bit i is set) on a
 * currently open menu level in place, then invalidates the changed rows.
 * Live equivalent of wuss_menu_tick_set. A no-op if \p handle is
 * stale/closed or \p menu is not an open level of its chain. \c menu->nitems
 * must not exceed the bit width of <tt>unsigned int</tt>.
 *
 * \param[in] handle Chain handle from wuss_menu_open.
 * \param[in] menu   The (sub)menu level to update; matched by pointer
 *                   against the description passed to wuss_menu_open or
 *                   reached via a wuss_menu_item_t.submenu.
 * \param[in] ticked Bit i ticks row i; 0 unticks every row.
 */
void wuss_menu_tick_set_live(wuss_menu_handle_t handle,
                             const wuss_menu_t *menu,
                             unsigned int       ticked);

/**
 * Set or clear a single item's tick on a currently open menu level in place,
 * leaving every other item's tick untouched. As
 * wuss_menu_tick_exclusive_live, but for a menu that tracks more than one
 * independent tick at the same level (e.g. a selection plus an unrelated
 * toggle) where unticking every other row would clobber state the caller
 * wanted to keep.
 *
 * \param[in] handle Chain handle from wuss_menu_open.
 * \param[in] menu   The (sub)menu level to update; matched by pointer
 *                   against the description passed to wuss_menu_open or
 *                   reached via a wuss_menu_item_t.submenu.
 * \param[in] index  Row to update. A no-op if out of range.
 * \param[in] ticked Non-zero to tick the row, zero to untick it.
 */
void wuss_menu_tick_item_live(wuss_menu_handle_t handle,
                              const wuss_menu_t *menu,
                              int                index,
                              int                ticked);

/* ----------------------------------------------------------------------- */

/* Building a wuss_menu_t tree from a compact descriptor string, and freeing
 * it again, lives in wuss/menu-desc.h -- a convenience layer on top of this
 * core helper. */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_MENU_H */
