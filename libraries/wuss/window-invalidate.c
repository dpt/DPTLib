/* window-invalidate.c -- wuss - minimal window manager */

#include <string.h>

#include "impl.h"

/* ponytail: fixed cap; if hit, remaining pieces are carried through
 * unclipped against further occluders rather than growing storage -- safe,
 * just some avoidable redraw work, never wrong */
#define WUSS_MAX_INVALIDATE_PIECES 32

/* Windows are treated as fully opaque over their whole visible footprint
 * (titlebar/outline included) for occlusion purposes, matching how
 * redraw_from repaints them: a client that leaves gaps via
 * wuss_NO_BACKGROUND won't get those gaps refreshed by this either. */

/* Append "piece" minus its intersection "cut" with an occluder to "out",
 * as up to four non-overlapping bands. */
static void box_subtract_into(const box_t *piece, const box_t *cut, box_t *out, int *pnout)
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
static int clip_to_visible(wuss_window_t *window, const box_t *box, box_t *out)
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

/* Invalidate "box" (screen space), clipped against windows above "window"
 * in the z-order: the shared primitive behind wuss_window_invalidate and
 * every other window-owned invalidation (move, resize, background change,
 * destroy) that would otherwise redraw parts nothing can see change. */
void wuss__invalidate_clipped(wuss_window_t *window, const box_t *box)
{
  box_t pieces[WUSS_MAX_INVALIDATE_PIECES];
  int   npieces, i;

  npieces = clip_to_visible(window, box, pieces);

  for (i = 0; i < npieces; i++)
    wuss_invalidate(window->wuss, &pieces[i]);
}

void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box)
{
  box_t screen_box, content;

  wuss__content_box(window, &content);

  screen_box.x0 = content.x0 + local_box->x0;
  screen_box.y0 = content.y0 + local_box->y0;
  screen_box.x1 = content.x0 + local_box->x1;
  screen_box.y1 = content.y0 + local_box->y1;

  wuss__invalidate_clipped(window, &screen_box);
}
