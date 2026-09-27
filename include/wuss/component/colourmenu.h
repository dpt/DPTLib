/* wuss/component/colourmenu.h -- a menu of the system palette colours */

/**
 * \file colourmenu.h
 *
 * A shared wuss component: a single process-wide pop-up menu with one row
 * per system-palette entry, each row carrying a colour chip (see
 * wuss_MENU_ITEM_SWATCH) -- the RISC OS Toolbox ColourMenu in miniature.
 *
 * There is one singleton instance, not a handle per caller. wuss_colourmenu_
 * menu() builds it (or rebuilds it, if \p wuss has changed since) and hands
 * back a plain wuss_menu_t (see wuss/menu.h): item i is palette index i, its
 * swatch that colour, its label a "#RRGGBB" hex string, plus a trailing
 * "None" row set off by a dashed separator and a hatched black/white chip in
 * place of a real colour, resolved by wuss_colourmenu_selected as
 * wuss_NO_BACKGROUND. The None row is shown by default; wuss_colourmenu_
 * set_none(0) hides it for a caller that only wants real colours.
 *
 * Because the menu is shared, two callers wanting it open at once (e.g. a
 * task offering both a Foreground and Background pick) cannot each have it
 * wired into their tree with a different title/None setting at the same time
 * -- retitle/reconfigure it and hand back wuss_colourmenu_menu() from a
 * wuss_EVENT_PRE_SUBMENU_OPEN handler instead, the way a single shared fg/bg
 * colourmenu already had to (see wuss/test/tasks/saturn.c).
 *
 * A task opens the menu with wuss_menu_open and, in its
 * wuss_EVENT_MENU_SELECT case, calls wuss_colourmenu_selected to turn the
 * event back into a wuss_colour_t palette index.
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
#include "framebuf/colour.h"

#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/**
 * The singleton menu to hand to wuss_menu_open, built (or rebuilt, if \p
 * wuss differs from the one it was last built against) covering \p wuss's
 * whole system palette: one row per entry, index order, each with its colour
 * chip and a "#RRGGBB" label, plus a trailing "None" row (see
 * wuss_colourmenu_set_none). The menu outlives changes to the palette --
 * pass the same \p wuss again after a wuss_set_palette to pick those changes
 * up.
 *
 * \param[in] wuss Owner; its palette is read now (not retained).
 * \return The menu, borrowed and valid until the next call that rebuilds it,
 *         or NULL if \p wuss is NULL or the build failed (OOM).
 */
const wuss_menu_t *wuss_colourmenu_menu(const wuss_t *wuss);

/**
 * Show or hide the trailing "None" row (shown by default). Affects every
 * caller sharing the singleton -- set it immediately before opening/handing
 * back the menu, e.g. from a wuss_EVENT_PRE_SUBMENU_OPEN handler.
 *
 * \param[in] with_none Non-zero to show the dashed-off "None" row (hatched
 *                      chip) resolving to wuss_NO_BACKGROUND; zero to hide
 *                      it.
 */
void wuss_colourmenu_set_none(int with_none);

/**
 * Change the singleton's titlebar caption -- the hook for retitling it (e.g.
 * from a wuss_EVENT_PRE_SUBMENU_OPEN handler) before handing it back as the
 * menu to open for a particular row.
 *
 * \param[in] title New caption, borrowed and copied; NULL for "Colour".
 * \return \ref result_OK, \ref result_OOM leaving the old title in place, or
 *         \ref result_NULL_ARG if the menu has not been built yet (call
 *         wuss_colourmenu_menu first).
 */
result_t wuss_colourmenu_set_title(const char *title);

/**
 * Tick the row for palette index \p colour, clearing every other row's tick
 * -- the hook for marking the current colour (e.g. from a
 * wuss_EVENT_PRE_SUBMENU_OPEN handler) before handing the menu back. Does
 * nothing if the menu has not been built yet.
 *
 * \param[in] colour Palette index to tick, wuss_NO_BACKGROUND for the "None"
 *                   row, or any other value to tick nothing.
 */
void wuss_colourmenu_set_ticked(wuss_colour_t colour);

/**
 * As wuss_colourmenu_set_ticked, for a caller holding an RGB colour rather
 * than a palette index: ticks the first palette row exactly matching \p
 * colour, or nothing if none does.
 *
 * \param[in] colour Colour to find in the palette.
 */
void wuss_colourmenu_set_ticked_rgb(colour_t colour);

/**
 * Resolve a wuss_EVENT_MENU_SELECT event to the picked palette index.
 *
 * Call from the task's wuss_EVENT_MENU_SELECT case. Returns 0 with \p ok
 * cleared -- not a match -- unless \p ev is a MENU_SELECT whose menu is the
 * colourmenu singleton's own. A match also moves the tick to the picked row
 * (see wuss_colourmenu_set_ticked), including on the rows of a menu an
 * ADJUST pick has left open.
 *
 * \param[in]  ev The event passed to the task's handle callback.
 * \param[out] ok Set non-zero if \p ev was the colourmenu's, else zero. May
 *                be NULL.
 * \return The selected palette index, wuss_NO_BACKGROUND for the "None" row,
 *         or 0 if \p ev is not the colourmenu's (check \p ok to tell that
 *         from a real index 0).
 */
wuss_colour_t wuss_colourmenu_selected(const wuss_event_t *ev, int *ok);

/**
 * Answer a wuss_EVENT_PRE_SUBMENU_OPEN by opening the singleton as that
 * row's submenu, set up for an RGB colour pick: None row hidden, retitled to
 * \p title and the row matching \p ticked ticked.
 *
 * \param[in] wuss             Owner, as for wuss_colourmenu_menu.
 * \param[in] ev               The PRE_SUBMENU_OPEN event being answered.
 * \param[in] title            Caption, as for wuss_colourmenu_set_title.
 * \param[in] ticked           Current colour, as for
 *                             wuss_colourmenu_set_ticked_rgb.
 * \return As wuss_menu_open_submenu_now, or \ref result_OOM if the menu
 *         could not be built.
 */
result_t wuss_colourmenu_open_rgb(const wuss_t       *wuss,
                                  const wuss_event_t *ev,
                                  const char         *title,
                                  colour_t            ticked);

/**
 * As wuss_colourmenu_selected, resolving the pick to an RGB colour from the
 * system palette.
 *
 * \param[in]  ev     The event passed to the task's handle callback.
 * \param[out] colour Set to the picked colour on a match; untouched
 *                    otherwise.
 * \return Non-zero if \p ev picked a real palette colour; zero if it is not
 *         the colourmenu's or picked the "None" row.
 */
int wuss_colourmenu_selected_rgb(const wuss_event_t *ev, colour_t *colour);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_COLOURMENU_H */
