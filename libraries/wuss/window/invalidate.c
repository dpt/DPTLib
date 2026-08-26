/* invalidate.c -- wuss - minimal window manager */

#include <string.h>

#include "../impl.h"

/* Windows are treated as fully opaque over their whole visible footprint
 * (titlebar/outline included) for occlusion purposes, matching how
 * redraw_from repaints them: a task that leaves gaps via
 * wuss_NO_BACKGROUND won't get those gaps refreshed by this either. */

/* Append "piece" minus its intersection "cut" with an occluder to "out",
 * as up to four non-overlapping bands. */
static void box_subtract_into(const box_t *piece,
                              const box_t *cut,
                              box_t       *out,
                              int         *pnout)
{
  if (cut->y0 > piece->y0 && *pnout < WUSS_MAX_INVALIDATE_PIECES)
  {
    out[*pnout].x0 = piece->x0; out[*pnout].y0 = piece->y0;
    out[*pnout].x1 = piece->x1; out[*pnout].y1 = cut->y0;
    (*pnout)++;
  }
  if (cut->y1 < piece->y1 && *pnout < WUSS_MAX_INVALIDATE_PIECES)
  {
    out[*pnout].x0 = piece->x0; out[*pnout].y0 = cut->y1;
    out[*pnout].x1 = piece->x1; out[*pnout].y1 = piece->y1;
    (*pnout)++;
  }
  if (cut->x0 > piece->x0 && *pnout < WUSS_MAX_INVALIDATE_PIECES)
  {
    out[*pnout].x0 = piece->x0; out[*pnout].y0 = cut->y0;
    out[*pnout].x1 = cut->x0;   out[*pnout].y1 = cut->y1;
    (*pnout)++;
  }
  if (cut->x1 < piece->x1 && *pnout < WUSS_MAX_INVALIDATE_PIECES)
  {
    out[*pnout].x0 = cut->x1;   out[*pnout].y0 = cut->y0;
    out[*pnout].x1 = piece->x1; out[*pnout].y1 = cut->y1;
    (*pnout)++;
  }
}

/* Clip "box" (screen space) down to the parts not already covered by
 * windows above "window" in the z-order, writing the surviving pieces to
 * "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and returning their count. */
int wuss__clip_to_visible(wuss_window_t *window, const box_t *box, box_t *out)
{
  box_t   scratch[WUSS_MAX_INVALIDATE_PIECES];
  box_t  *cur, *nxt, *tmp;
  int     ncur;
  list_t *e;

  cur    = out;
  nxt    = scratch;
  cur[0] = *box;
  ncur   = 1;

  for (e = window->wuss->z_order.next; e != &window->link; e = e->next)
  {
    wuss_window_t *occluder;
    int             nnext, p;

    occluder = (wuss_window_t *) e;
    nnext    = 0;

    for (p = 0; p < ncur; p++)
    {
      box_t cut;

      if (box_intersection(&occluder->visible, &cur[p], &cut))
      {
        if (nnext < WUSS_MAX_INVALIDATE_PIECES)
          nxt[nnext++] = cur[p]; /* no overlap: piece survives untouched */
      }
      else
      {
        box_subtract_into(&cur[p], &cut, nxt, &nnext);
      }
    }

    ncur = nnext;
    tmp  = cur;
    cur  = nxt;
    nxt  = tmp;

    if (ncur == 0)
      break;
  }

  if (cur != out)
    memcpy(out, cur, ncur * sizeof(*out));

  return ncur;
}

/* Subtract each of "cuts" (an array of "ncuts" boxes) from "whole", writing
 * the surviving pieces to "out" (capacity WUSS_MAX_INVALIDATE_PIECES) and
 * returning their count. */
static int subtract_boxes(const box_t *whole,
                          const box_t *cuts,
                          int          ncuts,
                          box_t       *out)
{
  box_t  scratch[WUSS_MAX_INVALIDATE_PIECES];
  box_t *cur, *nxt, *tmp;
  int    ncur, i;

  cur    = out;
  nxt    = scratch;
  cur[0] = *whole;
  ncur   = 1;

  for (i = 0; i < ncuts; i++)
  {
    int p, nnext;

    nnext = 0;

    for (p = 0; p < ncur; p++)
    {
      box_t cut;

      if (box_intersection(&cuts[i], &cur[p], &cut))
      {
        if (nnext < WUSS_MAX_INVALIDATE_PIECES)
          nxt[nnext++] = cur[p]; /* no overlap: piece survives untouched */
      }
      else
      {
        box_subtract_into(&cur[p], &cut, nxt, &nnext);
      }
    }

    ncur = nnext;
    tmp  = cur;
    cur  = nxt;
    nxt  = tmp;

    if (ncur == 0)
      break;
  }

  if (cur != out)
    memcpy(out, cur, ncur * sizeof(*out));

  return ncur;
}

/* Invalidate the parts of "window"'s footprint that are hidden behind other
 * windows at the current z-order -- the rest of its footprint is already
 * showing its own correct pixels, so redrawing that too would just be
 * wasted work. Bring-to-front calls this before reordering, so "hidden"
 * means "about to be uncovered"; send-to-back calls it after reordering, so
 * "hidden" means "just became covered" -- either way, invalidating exactly
 * these parts is enough for the next redraw to leave the screen correct. */
void wuss__invalidate_uncovered(wuss_window_t *window)
{
  box_t visible[WUSS_MAX_INVALIDATE_PIECES];
  box_t hidden[WUSS_MAX_INVALIDATE_PIECES];
  int   nvisible, nhidden, i;

  nvisible = wuss__clip_to_visible(window, &window->visible, visible);
  nhidden  = subtract_boxes(&window->visible, visible, nvisible, hidden);

  for (i = 0; i < nhidden; i++)
    wuss_invalidate(window->wuss, &hidden[i]);
}

/* Invalidate "box" (screen space), clipped against windows above "window"
 * in the z-order: the shared primitive behind wuss_window_invalidate and
 * every other window-owned invalidation (move, resize, background change,
 * destroy) that would otherwise redraw parts nothing can see change. */
void wuss__invalidate_clipped(wuss_window_t *window, const box_t *box)
{
  box_t pieces[WUSS_MAX_INVALIDATE_PIECES];
  int   npieces, i;

  npieces = wuss__clip_to_visible(window, box, pieces);

  for (i = 0; i < npieces; i++)
    wuss_invalidate(window->wuss, &pieces[i]);
}

/* Invalidate the part of "whole" not already covered by "keep" -- used
 * after a blit has slid a window's pixels from "whole" to "keep", so only
 * the vacated sliver still needs an actual repaint. */
void wuss__invalidate_minus(wuss_t *wuss, const box_t *whole, const box_t *keep)
{
  box_t pieces[WUSS_MAX_INVALIDATE_PIECES];
  box_t cut;
  int   npieces, i;

  if (box_intersection(keep, whole, &cut))
  {
    wuss_invalidate(wuss, whole); /* no overlap: all of "whole" is vacated */
    return;
  }

  npieces = 0;
  box_subtract_into(whole, &cut, pieces, &npieces);

  for (i = 0; i < npieces; i++)
    wuss_invalidate(wuss, &pieces[i]);
}

void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box)
{
  box_t screen_box, content, whole;

  wuss__content_box(window, &content);

  if (local_box == NULL)
  {
    whole.x0 = 0;
    whole.y0 = 0;
    whole.x1 = content.x1 - content.x0;
    whole.y1 = content.y1 - content.y0;
    local_box = &whole;
  }

  screen_box.x0 = content.x0 - window->scroll_x + local_box->x0;
  screen_box.y0 = content.y0 - window->scroll_y + local_box->y0;
  screen_box.x1 = content.x0 - window->scroll_x + local_box->x1;
  screen_box.y1 = content.y0 - window->scroll_y + local_box->y1;

  wuss__invalidate_clipped(window, &screen_box);
}
