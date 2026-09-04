/* wuss/window/invalidate.c -- wuss - minimal window manager */

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
int wuss__clip_to_visible(wuss_window_t *window,
                          const box_t   *box,
                          box_t         *out)
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

    occluder = wuss__window_from_link(e);
    if (occluder->flags & wuss_WINDOW_HIDDEN)
      continue; /* a hidden window occludes nothing */
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
int wuss__subtract_boxes(const box_t *whole,
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

/* Sequential single-rect blits (each a self-consistent memmove) can still
 * corrupt each other when one piece's destination lands on another piece's
 * still-unread source -- but that only actually matters if no blit order
 * avoids it. Build the "must happen before" graph (piece j before piece i
 * whenever dest[i] would overwrite clean[j]'s still-unread source) and
 * topologically sort it: any window with more than one occluder-carved
 * piece near a shared edge -- e.g. two bands split by a corner occluder --
 * routinely has one such pairwise overlap without there being a genuine
 * cycle, and rejecting those outright regressed plain corner-occlusion
 * drags into full fallback redraws. Only an actual cycle (i must precede j
 * and j must precede i) has no safe order and needs the fallback. */
int wuss__order_pieces(const box_t *clean,
                       const box_t *dest,
                       int          n,
                       int         *order)
{
  int adj[WUSS_MAX_INVALIDATE_PIECES][WUSS_MAX_INVALIDATE_PIECES];
  int indeg[WUSS_MAX_INVALIDATE_PIECES];
  int queue[WUSS_MAX_INVALIDATE_PIECES];
  int i, j, head, tail, nout, u;

  for (i = 0; i < n; i++)
    indeg[i] = 0;
  for (j = 0; j < n; j++)
    for (i = 0; i < n; i++)
      adj[j][i] = 0;

  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      if (i != j && !adj[j][i] && box_intersects(&dest[i], &clean[j]))
      {
        adj[j][i] = 1; /* j must be blitted before i */
        indeg[i]++;
      }

  tail = 0;
  for (i = 0; i < n; i++)
    if (indeg[i] == 0)
      queue[tail++] = i;

  head = nout = 0;
  while (head < tail)
  {
    u = queue[head++];
    order[nout++] = u;

    for (i = 0; i < n; i++)
      if (adj[u][i] && --indeg[i] == 0)
        queue[tail++] = i;
  }

  return nout == n;
}

/* Slide the pixels of "src" (nsrc clean pieces -- the caller has already
 * carved them clear of occluders and any stale regions) by (dx, dy),
 * clipping every destination against the screen and against the windows
 * above "window" so the blit never paints over an occluder. Blits are
 * clobber-ordered (wuss__order_pieces) so no piece overwrites another's
 * still-unread source. If "clip" is non-NULL scr->clip is pinned to it for
 * the duration (and restored after) -- pass it when the destinations must
 * not spill past some box (set-scroll pins the content box); pass NULL to
 * leave the clip alone and let screen_copy_rect self-clip to the screen.
 *
 * On success the pieces actually copied are written to "copied" (capacity
 * WUSS_MAX_INVALIDATE_PIECES), "*ncopied" is set, and 1 is returned -- the
 * caller then repaints whatever "copied" did not cover. Returns 0, with
 * "*ncopied" zeroed, when there is no safe fast path (nothing clean, the
 * piece budget overflowed, no clobber-free order exists, or screen_copy_rect
 * declined -- e.g. a paletted screen with no blit path): the caller must
 * fall back to invalidating its whole dirty region. A piece copied only
 * partially (its far edge slid off-screen) still has the uncovered remainder
 * invalidated here, since that ground was already clipped clear of every
 * occluder. A caller that already invalidates its whole affected region
 * against "copied" afterwards just sees those strips folded into the dirty
 * list twice, which is harmless. */
int wuss__blit_pieces(wuss_window_t *window,
                      const box_t   *src,
                      int            nsrc,
                      int            dx,
                      int            dy,
                      const box_t   *clip,
                      box_t         *copied,
                      int           *ncopied)
{
  box_t blit_src[WUSS_MAX_INVALIDATE_PIECES];
  box_t blit_dest[WUSS_MAX_INVALIDATE_PIECES];
  int   order[WUSS_MAX_INVALIDATE_PIECES];
  int   nblit, overflow, i, j, idx;
  box_t saved_clip;

  *ncopied = 0;

  nblit    = 0;
  overflow = 0;
  for (i = 0; i < nsrc && !overflow; i++)
  {
    box_t want, vis[WUSS_MAX_INVALIDATE_PIECES];
    int   nvis;

    box_translated(&src[i], dx, dy, &want);
    nvis = wuss__clip_to_visible(window, &want, vis);
    for (j = 0; j < nvis; j++)
    {
      if (nblit == WUSS_MAX_INVALIDATE_PIECES)
      {
        overflow = 1;
        break;
      }
      blit_dest[nblit] = vis[j];
      box_translated(&vis[j], -dx, -dy, &blit_src[nblit]);
      nblit++;
    }
  }

  if (nsrc == 0 || overflow ||
      !wuss__order_pieces(blit_src, blit_dest, nblit, order))
    return 0;

  if (clip != NULL)
  {
    saved_clip                = window->wuss->scr->clip;
    window->wuss->scr->clip   = *clip;
  }

  for (i = 0; i < nblit; i++)
  {
    box_t got;

    idx = order[i];
    if (screen_copy_rect(window->wuss->scr, &blit_src[idx],
                         POINT(blit_dest[idx].x0, blit_dest[idx].y0),
                         &got) != result_OK)
    {
      if (clip != NULL)
        window->wuss->scr->clip = saved_clip;
      *ncopied = 0;
      return 0;
    }

    /* "got" can be smaller than blit_dest[idx] if part slid off-screen: the
     * uncovered remainder has no source pixels and needs a real repaint.
     * Safe raw -- blit_dest[idx] was already clipped clear of occluders. */
    wuss__invalidate_minus(window->wuss, &blit_dest[idx], &got);

    if (*ncopied < WUSS_MAX_INVALIDATE_PIECES)
      copied[(*ncopied)++] = got;
  }

  if (clip != NULL)
    window->wuss->scr->clip = saved_clip;

  return 1;
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
  nhidden  = wuss__subtract_boxes(&window->visible, visible, nvisible, hidden);

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
void wuss__invalidate_minus(wuss_t      *wuss,
                            const box_t *whole,
                            const box_t *keep)
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

  box_translated(local_box, content.x0 - window->scroll.x,
                 content.y0 - window->scroll.y, &screen_box);

  wuss__invalidate_clipped(window, &screen_box);
}

void wuss_window_invalidate_extent(wuss_window_t *window)
{
  box_t extent;

  extent.x0 = 0;
  extent.y0 = 0;
  extent.x1 = window->doc.w;
  extent.y1 = window->doc.h;

  wuss_window_invalidate(window, &extent);
}
