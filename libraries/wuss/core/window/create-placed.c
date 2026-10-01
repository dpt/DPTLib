/* wuss/window/create-placed.c -- window creation with wuss-chosen position */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "geom/box.h"
#include "geom/packer.h"

#include "../impl.h"

/* Footprint padding around a content area of the given flags: the outline on
 * every edge, the titlebar on top, and the scrollbar/resize carve on the
 * right and bottom. Matches wuss_window_create's own visible-box maths so an
 * auto-placed slot ends up exactly the size the window will occupy. */
static void footprint_pad(const wuss_t       *wuss,
                          wuss_window_flags_t flags,
                          int                *left,
                          int                *top,
                          int                *right,
                          int                *bottom)
{
  int     outline_px, titlebar_height;
  point_t carve;

  outline_px      = wuss__outline_px_for(flags);
  titlebar_height = wuss__titlebar_height_for(wuss, flags);
  wuss__furniture_carve_for(flags, wuss__button_size_for(wuss, flags), &carve);

  *left   = outline_px;
  *top    = outline_px + titlebar_height;
  *right  = outline_px + carve.x;
  *bottom = outline_px + carve.y;
}

/* Centre a footprint of fw x fh on the screen, used once the layout packer
 * has no room. A footprint larger than the screen is pinned at the top-left
 * so its titlebar stays reachable. */
static void centre_on_screen(const wuss_t *wuss,
                             int           fw,
                             int           fh,
                             point_t      *pos)
{
  pos->x = MAX(0, (wuss->scr->size.w - fw) / 2);
  pos->y = MAX(0, (wuss->scr->size.h - fh) / 2);
}

/* Find a free spot for a footprint of fw x fh. The packer is rebuilt from
 * scratch around every shown window's current footprint on each call, so
 * windows created at explicit positions, moved or resized by the user, or a
 * changed screen size are all accounted for with no slot bookkeeping.
 * ponytail: O(windows^2) per placement; keep a live packer if that shows. */
static result_t find_slot(wuss_t *wuss, int fw, int fh, point_t *pos)
{
  static const box_t margins = {
    WUSS_PLACE_GUTTER, WUSS_PLACE_GUTTER, WUSS_PLACE_GUTTER, WUSS_PLACE_GUTTER
  };

  result_t       rc;
  box_t          screen;
  packer_t      *layout;
  list_t        *e;
  wuss_window_t *w;
  box_t          taken;
  const box_t   *slot;

  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = wuss->scr->size.w;
  screen.y1 = wuss->scr->size.h;

  layout = packer_create(&screen);
  if (layout == NULL)
    return result_OOM;

  packer_set_margins(layout, &margins);
  packer_set_gutter(layout, WUSS_PLACE_GUTTER);

  /* only regular windows constrain placement: the icon bar (back) and
   * error dialogues/menus (top) sit outside the middle stack */
  for (e = wuss->z_order[wuss_STACK_MIDDLE].next; e != NULL; e = e->next)
  {
    w = wuss__window_from_link(e);
    if (w->flags & wuss_WINDOW_HIDDEN)
      continue;

    /* gutter on the right/bottom only: packer_place_by already reserves one
     * on the new window's right/bottom, so padding all round would double it
     * and a freed slot would no longer fit its own window */
    taken.x0 = w->visible.x0;
    taken.y0 = w->visible.y0;
    taken.x1 = w->visible.x1 + WUSS_PLACE_GUTTER;
    taken.y1 = w->visible.y1 + WUSS_PLACE_GUTTER;

    rc = packer_place_at(layout, &taken);
    if (rc != result_OK && rc != result_PACKER_EMPTY) /* EMPTY: off-screen */
      goto exit;
  }

  /* packer's Y axis is reversed. bottom left here gives top left packing. */
  rc = packer_place_by(layout, packer_LOC_BOTTOM_LEFT, fw, fh, &slot);
  if (rc == result_OK)
  {
    pos->x = slot->x0;
    pos->y = slot->y0;
  }
  else if (rc == result_PACKER_DIDNT_FIT)
  {
    centre_on_screen(wuss, fw, fh, pos);
    rc = result_OK;
  }

exit:
  packer_destroy(layout);

  return rc;
}

result_t wuss_window_create_placed(wuss_task_t        *task,
                                   size2d_t            size,
                                   const char         *title,
                                   wuss_window_flags_t flags,
                                   wuss_backdrop_t     bg,
                                   size2d_t            doc,
                                   size2d_t            min_doc,
                                   wuss_window_t     **window)
{
  result_t rc;
  wuss_t  *wuss;
  int      left, top, right, bottom;
  point_t  origin;
  box_t    content;

  assert(task   != NULL);
  assert(window != NULL);

  wuss = task->wuss;

  if (!wuss__size_ok(size.w, size.h))
    return result_WUSS_TOO_SMALL;

  footprint_pad(wuss, flags, &left, &top, &right, &bottom);

  rc = find_slot(wuss,
                 left + size.w + right,
                 top  + size.h + bottom,
                 &origin);
  if (rc != result_OK)
    return rc;

  /* content box = footprint origin plus the top/left furniture padding */
  content.x0 = origin.x + left;
  content.y0 = origin.y + top;
  content.x1 = content.x0 + size.w;
  content.y1 = content.y0 + size.h;

  return wuss_window_create(task, &content, title, flags, bg,
                            doc, min_doc, window);
}
