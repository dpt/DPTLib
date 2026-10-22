/* wuss/iconbar/menu-open.c -- open a menu or window for an icon bar icon */

#include "../core/types.h"

#if defined(WUSS_ICONBAR) && defined(WUSS_MENUS)

result_t wuss_iconbar_menu_open(wuss_task_t        *task,
                                const wuss_menu_t  *menu,
                                wuss_menu_handle_t *out)
{
  return wuss_menu_open_above(task, menu, wuss_get_pointer(task->wuss).x,
                              WUSS_SAFE_INSET_BOTTOM, out);
}

result_t wuss_iconbar_window_open(wuss_task_t        *task,
                                  wuss_window_t      *window,
                                  wuss_menu_handle_t *out)
{
  box_t   visible;
  box_t   content;
  point_t at;

  wuss_window_get_visible_bounds(window, &visible);
  wuss_window_get_content_bounds(window, &content);

  /* the content top-left that puts the footprint where a menu's would be */
  at.x = wuss_get_pointer(task->wuss).x - (visible.x1 - visible.x0) / 2 +
         (content.x0 - visible.x0);
  at.y = task->wuss->scr->size.h - WUSS_SAFE_INSET_BOTTOM -
         (visible.y1 - visible.y0) + (content.y0 - visible.y0);

  return wuss_menu_open_window(task, window, at, out);
}

#endif /* WUSS_ICONBAR && WUSS_MENUS */
