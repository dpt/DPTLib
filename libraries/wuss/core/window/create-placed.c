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

/* The screen area a window may be placed in: the safe area, clear of the
 * screen edges and the icon bar, unless asked to ignore it. */
static void place_area(const wuss_t      *wuss,
                       wuss_place_flags_t flags,
                       box_t             *area)
{
  area->x0 = 0;
  area->y0 = 0;
  area->x1 = wuss->scr->size.w;
  area->y1 = wuss->scr->size.h;

  if ((flags & wuss_PLACE_IGNORE_SAFE_AREA) == 0)
  {
    area->x0 += WUSS_SAFE_INSET;
    area->y0 += WUSS_SAFE_INSET;
    area->x1 -= WUSS_SAFE_INSET;
    area->y1 -= WUSS_SAFE_INSET_BOTTOM;
  }
}

/* Position one axis of a footprint of length "len" within [lo,hi): against
 * the low or high edge, or centred. */
static int align_axis(int lo, int hi, int len, int at_lo, int at_hi)
{
  if (at_lo)
    return lo;
  else if (at_hi)
    return hi - len;
  else
    return lo + (hi - lo - len) / 2;
}

/* State for nearest_slot: the footprint and the top-left it would ideally
 * have. */
typedef struct nearest
{
  int     fw, fh;
  point_t want;
  point_t best;
  long    best_d2; /* squared distance of best from want; -1 if none */
}
nearest_t;

/* packer_map callback: the position in free region b closest to the wanted
 * one, kept if it is the closest seen so far */
static result_t nearest_slot(const box_t *b, void *opaque)
{
  nearest_t *n;
  point_t    p;
  long       dx, dy, d2;

  n = opaque;

  if (!box_could_hold(b, n->fw, n->fh))
    return result_OK;

  p.x = CLAMP(n->want.x, b->x0, b->x1 - n->fw);
  p.y = CLAMP(n->want.y, b->y0, b->y1 - n->fh);
  dx  = p.x - n->want.x;
  dy  = p.y - n->want.y;
  d2  = dx * dx + dy * dy;
  if (n->best_d2 < 0 || d2 < n->best_d2)
  {
    n->best    = p;
    n->best_d2 = d2;
  }

  return result_OK;
}

/* Find a free spot for a footprint of fw x fh within area. The packer is
 * rebuilt from scratch around every shown window's current footprint on each
 * call, so windows created at explicit positions, moved or resized by the
 * user, or a changed screen size are all accounted for with no slot
 * bookkeeping. Each window's footprint is grown by the gutter all round, so
 * the spot keeps clear of other windows by that but may sit flush with the
 * edges of area. Returns result_PACKER_DIDNT_FIT when no region is large
 * enough. The spot is the one closest to want, if non-NULL, else to the
 * corner of area that align names (top-left if it names none). self, if
 * non-NULL, is the window being placed, so is not in the way.
 * ponytail: O(windows^2) per placement; keep a live packer if that shows. */
static result_t find_slot(wuss_t              *wuss,
                          const wuss_window_t *self,
                          const box_t         *area,
                          int                  fw,
                          int                  fh,
                          wuss_align_t         align,
                          const point_t       *want,
                          point_t             *pos)
{
  result_t       rc;
  packer_t      *layout;
  list_t        *e;
  wuss_window_t *w;
  box_t          taken;
  nearest_t      n;

  layout = packer_create(area);
  if (layout == NULL)
    return result_OOM;

  /* only regular windows constrain placement: the icon bar (back) and
   * error dialogues/menus (top) sit outside the middle stack */
  for (e = wuss->z_order[wuss_STACK_MIDDLE].next; e != NULL; e = e->next)
  {
    w = wuss__window_from_link(e);
    if (w == self || (w->flags & wuss_WINDOW_HIDDEN))
      continue;

    taken.x0 = w->visible.x0 - WUSS_PLACE_GUTTER;
    taken.y0 = w->visible.y0 - WUSS_PLACE_GUTTER;
    taken.x1 = w->visible.x1 + WUSS_PLACE_GUTTER;
    taken.y1 = w->visible.y1 + WUSS_PLACE_GUTTER;

    rc = packer_place_at(layout, &taken);
    if (rc != result_OK && rc != result_PACKER_EMPTY) /* EMPTY: off-screen */
      goto exit;
  }

  n.fw = fw;
  n.fh = fh;
  if (want != NULL)
  {
    n.want = *want;
  }
  else
  {
    n.want.x = (align & wuss_ALIGN_RIGHT) ? area->x1 - fw : area->x0;
    n.want.y = (align & wuss_ALIGN_BOTTOM) ? area->y1 - fh : area->y0;
  }
  n.best_d2 = -1;

  rc = packer_map(layout, nearest_slot, &n);
  if (rc == result_OK)
  {
    if (n.best_d2 < 0)
      rc = result_PACKER_DIDNT_FIT;
    else
      *pos = n.best;
  }

exit:
  packer_destroy(layout);

  return rc;
}

/* Choose the top-left of a footprint of fw x fh within area as described by
 * place. self is the window being placed, if it is already open. */
static result_t place_footprint(wuss_t              *wuss,
                                const wuss_window_t *self,
                                const wuss_place_t  *place,
                                const box_t         *area,
                                int                  fw,
                                int                  fh,
                                point_t             *pos)
{
  result_t rc;
  point_t  ptr;
  point_t  want;

  /* the footprint centred on the pointer */
  ptr    = wuss_get_pointer(wuss);
  want.x = ptr.x - fw / 2;
  want.y = ptr.y - fh / 2;

  if (place->flags & wuss_PLACE_SPARE)
  {
    rc = find_slot(wuss, self, area, fw, fh, place->align,
                   (place->flags & wuss_PLACE_POINTER) ? &want : NULL, pos);
    if (rc != result_PACKER_DIDNT_FIT)
      return rc; /* placed, or failed */
  }

  if (place->relative_to != NULL)
  {
    pos->x = place->relative_to->visible.x0;
    pos->y = place->relative_to->visible.y0;
  }
  else if (place->flags & wuss_PLACE_POINTER)
  {
    *pos = want;
  }
  else
  {
    pos->x = align_axis(area->x0, area->x1, fw,
                        place->align & wuss_ALIGN_LEFT,
                        place->align & wuss_ALIGN_RIGHT);
    pos->y = align_axis(area->y0, area->y1, fh,
                        place->align & wuss_ALIGN_TOP,
                        place->align & wuss_ALIGN_BOTTOM);
  }

  pos->x += place->offset.x;
  pos->y += place->offset.y;

  /* fit the area, keeping the top-left (titlebar, close icon) reachable when
   * the footprint is larger than it */
  pos->x = MAX(area->x0, MIN(pos->x, area->x1 - fw));
  pos->y = MAX(area->y0, MIN(pos->y, area->y1 - fh));

  return result_OK;
}

result_t wuss_window_create_at(wuss_task_t        *task,
                               const wuss_place_t *place,
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
  box_t    area;
  point_t  origin;
  box_t    content;

  assert(task   != NULL);
  assert(place  != NULL);
  assert(window != NULL);

  wuss = task->wuss;

  if (!wuss__size_ok(size.w, size.h))
    return result_WUSS_TOO_SMALL;

  footprint_pad(wuss, flags, &left, &top, &right, &bottom);
  left   += place->extra.x0;
  top    += place->extra.y0;
  right  += place->extra.x1;
  bottom += place->extra.y1;
  place_area(wuss, place->flags, &area);

  /* shrink a window too large for the area to fit it; the document keeps its
   * full extent, so the rest is reached by scrolling */
  size.w = MAX(1, MIN(size.w, area.x1 - area.x0 - left - right));
  size.h = MAX(1, MIN(size.h, area.y1 - area.y0 - top - bottom));

  rc = place_footprint(wuss,
                       NULL,
                       place,
                       &area,
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

result_t wuss_window_place(wuss_window_t      *window,
                           const wuss_place_t *place)
{
  result_t rc;
  wuss_t  *wuss;
  box_t    content;
  box_t    area;
  point_t  origin;

  assert(window != NULL);
  assert(place  != NULL);

  wuss = window->wuss;
  wuss__content_box(window, &content);

  place_area(wuss, place->flags, &area);

  rc = place_footprint(wuss,
                       window,
                       place,
                       &area,
                       place->extra.x0 +
                         (window->visible.x1 - window->visible.x0) +
                         place->extra.x1,
                       place->extra.y0 +
                         (window->visible.y1 - window->visible.y0) +
                         place->extra.y1,
                       &origin);
  if (rc != result_OK)
    return rc;

  /* content top-left keeps its offset from the footprint's */
  wuss_window_move(window,
                   POINT(origin.x + place->extra.x0 +
                           (content.x0 - window->visible.x0),
                         origin.y + place->extra.y0 +
                           (content.y0 - window->visible.y0)));

  return result_OK;
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
  static const wuss_place_t spare = { wuss_PLACE_SPARE, wuss_ALIGN_CENTRE,
                                       NULL, { 0, 0 }, { 0, 0, 0, 0 } };

  return wuss_window_create_at(task, &spare, size, title, flags, bg,
                               doc, min_doc, window);
}
