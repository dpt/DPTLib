/* wuss/iconbar/menu-open.c -- open a menu for an icon bar icon */

#include "../core/impl.h"

#if defined(WUSS_ICONBAR) && defined(WUSS_MENUS)

/* Distance from the bottom edge of an icon bar menu to the bottom of the
 * screen, so every icon's menu sits at the same height. */
#define WUSS_ICONBAR_MENU_GAP 47

result_t wuss_iconbar_menu_open(wuss_task_t        *task,
                                const wuss_menu_t  *menu,
                                wuss_menu_handle_t *out)
{
  return wuss_menu_open_above(task, menu, wuss_get_pointer(task->wuss).x,
                              WUSS_ICONBAR_MENU_GAP, out);
}

#endif /* WUSS_ICONBAR && WUSS_MENUS */
