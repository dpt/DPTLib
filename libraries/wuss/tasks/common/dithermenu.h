/* wuss/tasks/common/dithermenu.h -- "Dithering" submenu of screen_dither_t */

#ifndef TASKS_DITHERMENU_H
#define TASKS_DITHERMENU_H

#include "base/utils.h"
#include "framebuf/screen.h"
#include "wuss/menu.h"

/* a "Dithering" submenu with one row per screen_dither_t, for a task to
 * embed and borrow from its main menu */
typedef struct dithermenu
{
  wuss_menu_item_t items[3]; /* indexed by screen_dither_t */
  wuss_menu_t      menu;
}
dithermenu_t;

/* build dm's rows and tick the one for dithering */
static inline void dithermenu_init(dithermenu_t   *dm,
                                   screen_dither_t dithering)
{
  WUSS_MENU_ITEM(dm->items, screen_DITHER_NEAREST, "Nearest",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(dm->items, screen_DITHER_BAYER, "Bayer",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(dm->items, screen_DITHER_BLUE_NOISE, "Blue noise",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(dm->menu, "Dithering", dm->items, NELEMS(dm->items));
  wuss_menu_tick_exclusive(&dm->menu, (int) dithering);
}

/* tick row index, both in dm and in the open chain handle, and return the
 * method it names */
static inline screen_dither_t dithermenu_select(dithermenu_t      *dm,
                                                wuss_menu_handle_t handle,
                                                int                index)
{
  wuss_menu_tick_exclusive(&dm->menu, index);
  wuss_menu_tick_exclusive_live(handle, &dm->menu, index);

  return (screen_dither_t) index;
}

#endif /* TASKS_DITHERMENU_H */
