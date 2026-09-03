/* wuss/component/fontmenu.h -- a menu of the available bitmap fonts */

/**
 * \file fontmenu.h
 *
 * A shared wuss component: a pop-up menu listing the bitmap fonts in a
 * directory, the RISC OS Toolbox FontMenu in miniature.
 *
 * A wuss_fontmenu owns a plain wuss_menu_t (see wuss/menu.h) built from
 * bmfont_enumerate: one leaf item per font, alphabetically sorted, its label
 * the font's leafname sans ".png". A task opens it with wuss_menu_open and,
 * in its wuss_EVENT_MENU_SELECT case, calls wuss_fontmenu_selected to turn
 * the event back into a font name -- no indexing into menu->items by hand.
 *
 * Built only when WUSS_COMPONENTS is defined (which implies WUSS_MENUS).
 */

#ifndef WUSS_COMPONENT_FONTMENU_H
#define WUSS_COMPONENT_FONTMENU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/menu.h"
#include "wuss/task.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: owns the menu tree and the font-name strings in it. */
typedef struct wuss_fontmenu wuss_fontmenu_t;

/**
 * Build a font menu from the ".png" fonts in \p dir (see bmfont_enumerate).
 * Items are sorted by name; \p title is the menu's caption (NULL -> "Font").
 *
 * \param[out] out   Filled with the new handle on success, untouched on
 *                   failure.
 * \param[in]  dir   Directory to scan for fonts.
 * \param[in]  title Menu caption, borrowed and copied; NULL for "Font".
 * \return \ref result_OK, \ref result_OOM, \ref result_NULL_ARG, or \ref
 *         result_FILE_NOT_FOUND if \p dir cannot be opened. An empty
 *         directory still succeeds, yielding a menu with no items.
 */
result_t wuss_fontmenu_create(wuss_fontmenu_t **out,
                              const char       *dir,
                              const char       *title);

/**
 * Free a font menu and every string in it. Any open menu chain showing it
 * must be closed first (wuss_menu_close). Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_fontmenu_destroy(wuss_fontmenu_t *doomed);

/**
 * The menu to hand to wuss_menu_open. Borrowed; valid until
 * wuss_fontmenu_destroy. NULL only if \p fm is NULL.
 *
 * \param[in] fm Handle.
 * \return The menu, or NULL.
 */
const wuss_menu_t *wuss_fontmenu_menu(const wuss_fontmenu_t *fm);

/**
 * Resolve a wuss_EVENT_MENU_SELECT event to the picked font's name.
 *
 * Call from the task's wuss_EVENT_MENU_SELECT case. Returns NULL -- not a
 * match -- unless \p ev is a MENU_SELECT whose menu is this fontmenu's own;
 * so a task multiplexing several menus through one handle can call each
 * fontmenu's resolver in turn.
 *
 * \param[in] fm Handle.
 * \param[in] ev The event passed to the task's handle callback.
 * \return The selected font's name (borrowed, valid until
 *         wuss_fontmenu_destroy), or NULL if \p ev is not this fontmenu's.
 */
const char *wuss_fontmenu_selected(const wuss_fontmenu_t *fm,
                                   const wuss_event_t    *ev);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_FONTMENU_H */
