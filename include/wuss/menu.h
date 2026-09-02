/* wuss/menu.h -- wuss pop-up menu helper */

/**
 * \file menu.h
 *
 * A thin helper for RISC OS-style pop-up menus: a menu is a borderless
 * window populated with wuss_ICON_TYPE_MENU_ENTRY icons, but wuss owns the
 * plumbing -- layout, placement, submenu chaining on hover and whole-chain
 * dismissal on a click outside or a leaf selection.
 *
 * Menus are described by caller-owned, immutable wuss_menu_t /
 * wuss_menu_item_t structures (which may be static). The helper never
 * mutates them; a task that wants a tick to change just edits its own array
 * and reopens the menu.
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
  wuss_MENU_ITEM_NONE      = 0,
  wuss_MENU_ITEM_TICKED    = 1 << 0, /**< draw a tick at the item's left edge */
  wuss_MENU_ITEM_DASHED    = 1 << 1, /**< draw a dashed rule above this item,
                                      *   marking a group boundary. The item is
                                      *   otherwise an ordinary row: it keeps
                                      *   its label and responds to the pointer.
                                      *   The rule is laid out and drawn
                                      *   separately and is not interactive */
  wuss_MENU_ITEM_DISABLED  = 1 << 2  /**< greyed, never highlights, not
                                      *   selectable */
}
wuss_menu_item_flags_t;

/** One row of a menu. */
typedef struct wuss_menu_item
{
  const char             *text;    /**< row label; NULL is treated as "" */
  wuss_menu_item_flags_t   flags;  /**< see wuss_menu_item_flags_t */
  const struct wuss_menu  *submenu; /**< non-NULL: draw a right arrow and open
                                     *   this menu to the right on hover */
  wuss_window_t           *window; /**< non-NULL: draw a right arrow and, on
                                     *   hover, show this caller-owned window
                                     *   where a submenu would open, hiding it
                                     *   again when the pointer leaves the row
                                     *   or the chain is dismissed. Create it
                                     *   with wuss_WINDOW_HIDDEN. Mutually
                                     *   exclusive with \c submenu. The window
                                     *   must outlive the open chain -- do not
                                     *   wuss_window_close it while its menu is
                                     *   open. */
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

/* ----------------------------------------------------------------------- */

/**
 * Build a wuss_menu_t tree from a compact descriptor string. The syntax is
 * lifted from PrivateEye's menu_create_from_desc. A comma separates items.
 * The very first token is the root menu's titlebar caption, not an item --
 * exactly as the first token inside a '{ }' is that submenu's title -- so
 * the same descriptor strings port across unchanged. Submenus keep the Wimp
 * behaviour of discarding their title token; only the root's is kept. A
 * leading '|' on an item draws a dashed rule above it, marking a group
 * boundary; the item itself stays an ordinary interactive row. A '{ ... }'
 * group after an item is that item's submenu. A per-token prefix '!' ticks
 * the item, '~' shades (disables) it, and '>' attaches a submenu pulled as a
 * <tt>const wuss_menu_t *</tt> from the varargs rather than from a following
 * '{ }' block. A "%s" in a token substitutes the next <tt>const char *</tt>
 * vararg. The '>' and "%s" varargs are consumed in the order they are
 * encountered scanning left to right.
 *
 * Example: <tt>wuss_menu_create_from_desc(&m, "Display, Open, !Grid,
 * ~Export, |Quit")</tt> -- "Display" is the caption; the menu has four
 * items, with a dashed rule above "Quit".
 *
 * The whole tree, including copied label text, is one heap allocation graph
 * owned by the caller; free it with wuss_menu_destroy. wuss_menu_open treats
 * a desc-built tree exactly like a static literal.
 *
 * \param[out] out  Filled with the root menu on success, untouched on
 *                  failure.
 * \param[in]  desc Descriptor string; its first token is the root caption.
 * \return \ref result_OK, \ref result_OOM, or \ref result_BAD_ARG for a
 *         malformed descriptor (unbalanced braces, empty token, too deep).
 */
result_t wuss_menu_create_from_desc(wuss_menu_t **out, const char *desc, ...);

/**
 * Free a tree built by wuss_menu_create_from_desc, including every submenu
 * and copied label. Safe to pass NULL. Never call this on a static or
 * caller-assembled wuss_menu_t.
 *
 * \param[in] menu Root menu returned by wuss_menu_create_from_desc, or NULL.
 */
void wuss_menu_destroy(wuss_menu_t *menu);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_MENU_H */
