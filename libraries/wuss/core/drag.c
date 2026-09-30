/* wuss/drag.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

/* Invalidate just the four 1px edges of "box", not its filled interior --
 * cheap enough to call twice a move (old position, new position) without
 * repainting whatever the ants box covers each time. */
static void invalidate_edges(wuss_t *wuss, const box_t *box)
{
  wuss_invalidate(wuss, &(box_t) BOX_POS_SIZE(box->x0, box->y0,
                                              box->x1 - box->x0, 1));
  wuss_invalidate(wuss, &(box_t) BOX_POS_SIZE(box->x0, box->y1 - 1,
                                              box->x1 - box->x0, 1));
  wuss_invalidate(wuss, &(box_t) BOX_POS_SIZE(box->x0, box->y0,
                                              1, box->y1 - box->y0));
  wuss_invalidate(wuss, &(box_t) BOX_POS_SIZE(box->x1 - 1, box->y0,
                                              1, box->y1 - box->y0));
}

static void box_for_pointer(const wuss_t *wuss, point_t p, box_t *out)
{
  *out = (box_t) BOX_POS_SIZE(p.x - wuss->drag_hotspot.x,
                              p.y - wuss->drag_hotspot.y,
                              wuss->drag_size.w, wuss->drag_size.h);
}

result_t wuss_drag_start(wuss_t        *wuss,
                         wuss_window_t *window,
                         size2d_t       size,
                         point_t        hotspot)
{
  assert(wuss != NULL);

  if (window == NULL || window->task->handle == NULL ||
      size.w <= 0 || size.h <= 0)
    return result_BAD_ARG;

  wuss->drag_window  = window;
  wuss->drag_size    = size;
  wuss->drag_hotspot = hotspot;
  wuss->drag_frame   = 0;
  box_for_pointer(wuss, wuss->pointer, &wuss->drag_box);
  invalidate_edges(wuss, &wuss->drag_box);

  return result_OK;
}

/* Called from mouse-move.c on every move while a drag is active: recompute
 * the ants box at the new pointer position, invalidating the edges it
 * leaves and the edges it now occupies. */
void wuss__drag_move(wuss_t *wuss, point_t p)
{
  box_t was;

  was = wuss->drag_box;
  box_for_pointer(wuss, p, &wuss->drag_box);
  invalidate_edges(wuss, &was);
  invalidate_edges(wuss, &wuss->drag_box);
}

/* Called from mouse-click.c (a MOUSE_UP) and key.c (Escape) to end an active
 * drag: settles deferred POINTER_ENTER/EXIT, invalidates the ants box's last
 * position and delivers wuss_EVENT_DRAG_END. "drop" is the window under the
 * pointer, ignored (and reported NULL) when cancelled. */
void wuss__drag_end(wuss_t        *wuss,
                    point_t        p,
                    wuss_window_t *drop,
                    int            cancelled)
{
  wuss_window_t *window;
  wuss_event_t   event;

  window = wuss->drag_window;

  invalidate_edges(wuss, &wuss->drag_box);
  wuss->drag_window = NULL;

  wuss__pointer_set_window(wuss, cancelled ? NULL : drop);

  event.kind                  = wuss_EVENT_DRAG_END;
  event.data.drag_end.drop    = cancelled ? NULL : drop;
  event.data.drag_end.point   = p;
  event.data.drag_end.cancelled = cancelled;
  (void) wuss__deliver(window->task, window, &event);
}

/* Called from redraw.c, painted last so the ants sit over every window.
 * Each edge starts its dash pattern "phase" steps in, walking the pattern
 * one step further round the box every frame so it appears to march;
 * corners keep the phase continuous rather than each edge restarting at 0. */
void wuss__drag_draw(wuss_t *wuss)
{
  colour_t black;
  box_t    box, saved_clip, clipped;
  int      period, phase;

  if (wuss->drag_window == NULL)
    return;

  black  = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_BLACK)];
  box    = wuss->drag_box;
  period = WUSS_DRAG_ANTS_ON + WUSS_DRAG_ANTS_OFF;
  phase  = wuss->drag_frame % period;

  /* Each dash pattern is anchored outside "box" (box.x0 - phase, etc.) so it
   * marches continuously; screen_draw_dashed_line only clips to the screen
   * and the caller's existing clip, neither of which stops it painting past
   * the box's own edge into pixels invalidate_edges() never marks dirty.
   * Pin the clip to box, intersected with whatever the caller (wuss_redraw /
   * wuss_redraw_dirty) already set, and restore it afterwards. */
  saved_clip = wuss->scr->clip;
  if (box_intersection(&box, &saved_clip, &clipped))
    return; /* box is wholly outside the redraw region */
  wuss->scr->clip = clipped;

  screen_draw_dashed_line(wuss->scr, box.x0 - phase, box.y0, box.x1 - 1, box.y0,
                          WUSS_DRAG_ANTS_ON, WUSS_DRAG_ANTS_OFF, black);
  screen_draw_dashed_line(wuss->scr, box.x1 - 1 + phase, box.y1 - 1, box.x0, box.y1 - 1,
                          WUSS_DRAG_ANTS_ON, WUSS_DRAG_ANTS_OFF, black);
  screen_draw_dashed_line(wuss->scr, box.x0, box.y1 - 1 + phase, box.x0, box.y0,
                          WUSS_DRAG_ANTS_ON, WUSS_DRAG_ANTS_OFF, black);
  screen_draw_dashed_line(wuss->scr, box.x1 - 1, box.y0 - phase, box.x1 - 1, box.y1 - 1,
                          WUSS_DRAG_ANTS_ON, WUSS_DRAG_ANTS_OFF, black);

  wuss->scr->clip = saved_clip;
}
