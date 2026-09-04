/* wuss/menu-desc.h -- build a wuss_menu_t tree from a descriptor string */

/**
 * \file menu-desc.h
 *
 * A convenience helper on top of wuss/menu.h: parse a compact
 * PrivateEye-style descriptor string into a heap wuss_menu_t tree the caller
 * owns, and free it again. Nothing in the core menu helper depends on this;
 * a task that hand-assembles its wuss_menu_t arrays never needs to include it.
 *
 * Built only when WUSS_MENUS is defined.
 */

#ifndef WUSS_MENU_DESC_H
#define WUSS_MENU_DESC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/menu.h"

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

#endif /* WUSS_MENU_DESC_H */
