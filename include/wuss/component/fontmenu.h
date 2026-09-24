/* wuss/component/fontmenu.h -- a menu of the available bitmap fonts */

/**
 * \file fontmenu.h
 *
 * A shared wuss component: a single process-wide pop-up menu listing the
 * bitmap fonts in a directory, the RISC OS Toolbox FontMenu in miniature.
 *
 * There is one singleton instance, not a handle per caller. wuss_fontmenu_
 * menu() builds it (or rebuilds it, if \p dir or \p wuss differ from the
 * call that built it) and hands back a plain wuss_menu_t (see wuss/menu.h):
 * one leaf item per font, alphabetically sorted, its label the font's
 * leafname sans ".png". A task opens it with wuss_menu_open and, in its
 * wuss_EVENT_MENU_SELECT case, calls wuss_fontmenu_selected to turn the
 * event back into a font name -- no indexing into menu->items by hand.
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
#include "wuss/wuss.h"

/* ----------------------------------------------------------------------- */

/**
 * The singleton menu to hand to wuss_menu_open, built (or rebuilt, if \p dir
 * or \p wuss differ from the call that built it) from the ".png" fonts in \p
 * dir (see bmfont_enumerate): one leaf item per font, alphabetically sorted,
 * its label the font's leafname sans ".png".
 *
 * Because the menu is shared, two callers wanting different directories (or
 * different \p wuss skip-lists) open at once cannot each have it wired into
 * their tree at the same time -- rebuild it from a wuss_EVENT_PRE_SUBMENU_
 * OPEN handler instead, the way the colourmenu singleton's fg/bg split does
 * (see wuss/test/tasks/text.c).
 *
 * \param[in] dir   Directory to scan for fonts.
 * \param[in] title Menu caption, borrowed and copied; NULL for "Font".
 * \param[in] wuss  Window manager to consult for fonts to leave out, or NULL
 *                  to include every font found. Any wuss_create font slot
 *                  (see \ref wuss_font_desc_t) whose class is \ref
 *                  wuss_FONT_CLASS_SYSTEM and whose name matches a scanned
 *                  leafname is skipped -- e.g. the symbol font wuss itself
 *                  draws menu ticks and submenu arrows from, which is not
 *                  meant to be picked as a text font.
 * \return The menu, borrowed and valid until the next call that rebuilds it,
 *         or NULL if \p dir is NULL or the build failed (OOM or \p dir could
 *         not be opened).
 */
const wuss_menu_t *wuss_fontmenu_menu(const char   *dir,
                                      const char   *title,
                                      const wuss_t *wuss);

/**
 * Resolve a wuss_EVENT_MENU_SELECT event to the picked font's name.
 *
 * Call from the task's wuss_EVENT_MENU_SELECT case. Returns NULL -- not a
 * match -- unless \p ev is a MENU_SELECT whose menu is the fontmenu
 * singleton's own.
 *
 * \param[in] ev The event passed to the task's handle callback.
 * \return The selected font's name (borrowed, valid until the next call that
 *         rebuilds the singleton), or NULL if \p ev is not the fontmenu's.
 */
const char *wuss_fontmenu_selected(const wuss_event_t *ev);

/**
 * Tick the item at \p index and untick every other item, so the menu shows
 * which font is currently in use. The menu is caller-displayed, not redrawn
 * here -- call before wuss_menu_open (or reopen the menu) for the tick to be
 * seen. Affects every caller sharing the singleton.
 *
 * \param[in] index Row to tick, or -1 to untick every row (e.g. when a font
 *                  outside the menu, such as the wuss system font, is in
 *                  use). Out-of-range values other than -1 still untick
 *                  every row. A no-op if the singleton has not been built
 *                  yet (call wuss_fontmenu_menu first).
 */
void wuss_fontmenu_set_ticked(int index);

#ifdef __cplusplus
}
#endif

#endif /* WUSS_COMPONENT_FONTMENU_H */
