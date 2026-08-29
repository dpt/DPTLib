/* create-placed.c -- wuss - window creation with wuss-chosen position */

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
static void footprint_pad(const wuss_t        *wuss,
                          wuss_window_flags_t  flags,
                          int                 *left,
                          int                 *top,
                          int                 *right,
                          int                 *bottom)
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

/* Pick the next cascade position for a window of the given footprint size,
 * once the layout packer has no room. Steps down/right by a titlebar each
 * call, wrapping back to the top-left when the step would push the footprint
 * off the screen. */
static void next_cascade(wuss_t *wuss, int fw, int fh, point_t *pos)
{
  int scr_w, scr_h, step;

  scr_w = wuss->scr->size.w;
  scr_h = wuss->scr->size.h;
  step  = wuss->titlebar_height;
  if (step <= 0)
    step = WUSS_DEFAULT_TITLEBAR_HEIGHT;

  if (wuss->cascade.x + fw > scr_w || wuss->cascade.y + fh > scr_h)
  {
    wuss->cascade.x = 0;
    wuss->cascade.y = 0;
  }

  *pos = wuss->cascade;

  wuss->cascade.x += step;
  wuss->cascade.y += step;
}

result_t wuss_window_create_placed(wuss_t             *wuss,
                                   size2d_t            size,
                                   const char         *title,
                                   wuss_window_flags_t flags,
                                   wuss_colour_t       bg,
                                   const wuss_task_t  *task,
                                   size2d_t            doc,
                                   size2d_t            min_doc,
                                   wuss_window_t     **window)
{
  result_t       rc;
  int            left, top, right, bottom;
  int            fw, fh;
  box_t          screen, content;
  point_t        origin;
  const box_t   *slot;
  int            tracked;

  assert(wuss   != NULL);
  assert(window != NULL);

  if (!wuss__size_ok(size.w, size.h))
    return result_WUSS_TOO_SMALL;

  if (wuss->layout == NULL)
  {
    screen.x0 = 0;
    screen.y0 = 0;
    screen.x1 = wuss->scr->size.w;
    screen.y1 = wuss->scr->size.h;

    wuss->layout = packer_create(&screen);
    if (wuss->layout == NULL)
      return result_OOM;
  }

  footprint_pad(wuss, flags, &left, &top, &right, &bottom);
  fw = left + size.w + right;
  fh = top  + size.h + bottom;

  /* packer's Y axis is reversed. bottom left here gives top left packing. */
  rc = packer_place_by(wuss->layout, packer_LOC_BOTTOM_LEFT, fw, fh, &slot);
  if (rc == result_OK)
  {
    origin.x = slot->x0;
    origin.y = slot->y0;
    tracked  = 1;
  }
  else if (rc == result_PACKER_DIDNT_FIT)
  {
    next_cascade(wuss, fw, fh, &origin);
    tracked = 0;
  }
  else
  {
    return rc;
  }

  /* content box = footprint origin plus the top/left furniture padding */
  content.x0 = origin.x + left;
  content.y0 = origin.y + top;
  content.x1 = content.x0 + size.w;
  content.y1 = content.y0 + size.h;

  rc = wuss_window_create(wuss, &content, title, flags, bg, task,
                          doc, min_doc, window);
  if (rc != result_OK)
  {
    if (tracked)
    {
      box_t placed;

      placed.x0 = origin.x;
      placed.y0 = origin.y;
      placed.x1 = origin.x + fw;
      placed.y1 = origin.y + fh;
      (void) packer_release(wuss->layout, &placed);
    }
    return rc;
  }

  if (tracked)
    (*window)->packed = *slot;

  return result_OK;
}
