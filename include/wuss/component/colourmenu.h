/* wuss/component/colourmenu.h -- a menu of the system palette colours */

/**
 * \file colourmenu.h
 *
 * A shared wuss component: a pop-up menu with one row per system-palette
 * entry, each row carrying a colour chip (see wuss_MENU_ITEM_SWATCH) -- the
 * RISC OS Toolbox ColourMenu in miniature.
 *
 * A wuss_colourmenu owns a plain wuss_menu_t (see wuss/menu.h): item i is
 * palette index i, its swatch that colour, its label a "#RRGGBB" hex string.
 * A task opens it with wuss_menu_open and, in its wuss_EVENT_MENU_SELECT
 * case, calls wuss_colourmenu_selected to turn the event back into a
 * wuss_colour_t palette index.
 *
 * Built only when WUSS_COMPONENTS is defined (which implies WUSS_MENUS).
 */

#ifndef WUSS_COMPONENT_COLOURMENU_H
#define WUSS_COMPONENT_COLOURMENU_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/** Opaque handle: owns the menu tree and the hex-label strings in it. */
typedef struct wuss_colourmenu wuss_colourmenu_t;

/**
 * Build a colour menu covering \p wuss's whole system palette: one row per
 * entry, index order, each with its colour chip and a "#RRGGBB" label.
 *
 * \param[out] out   Filled with the new handle on success, untouched on
 *                   failure.
 * \param[in]  wuss  Owner; its palette is read now (not retained). The menu
 *                   outlives changes to the palette -- rebuild it after a
 *                   wuss_set_palette if the rows should follow.
 * \param[in]  title Menu caption, borrowed and copied; NULL for "Colour".
 * \return \ref result_OK, \ref result_OOM, or \ref result_NULL_ARG.
 */
result_t wuss_colourmenu_create(wuss_colourmenu_t **out,
                                const wuss_t       *wuss,
                                const char         *title);

/**
 * Free a colour menu and every string in it. Any open menu chain showing it
 * must be closed first (wuss_menu_close). Safe to pass NULL.
 *
 * \param[in] doomed Handle to free, or NULL.
 */
void wuss_colourmenu_destroy(wuss_colourmenu_t *doomed);

/**
 * The menu to hand to wuss_menu_open. Borrowed; valid until
 * wuss_colourmenu_destroy. NULL only if \p cm is NULL.
 *
 * \param[in] cm Handle.
 * \return The menu, or NULL.
 */
const wuss_menu_t *wuss_colourmenu_menu(const wuss_colourmenu_t *cm);

/**
 * Resolve a wuss_EVENT_MENU_SELECT event to the picked palette index.
 *
 * Call from the task's wuss_EVENT_MENU_SELECT case. Returns 0 with \p ok
 * cleared -- not a match -- unless \p ev is a MENU_SELECT whose menu is this
 * colourmenu's own; so a task multiplexing several menus through one handle
 * can call each resolver in turn.
 *
 * \param[in]  cm Handle.
 * \param[in]  ev The event passed to the task's handle callback.
 * \param[out] ok Set non-zero if \p ev was this colourmenu's, else zero. May
 *                be NULL.
 * \return The selected palette index, or 0 if \p ev is not this colourmenu's
 *         (check \p ok to tell that from a real index 0).
 */
wuss_colour_t wuss_colourmenu_selected(const wuss_colourmenu_t *cm,
                                       const wuss_event_t      *ev,
                                       int                     *ok);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_COLOURMENU_H */
