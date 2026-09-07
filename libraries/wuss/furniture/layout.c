/* wuss/furniture/layout.c -- wuss - minimal window manager */

/* Precompute the fixed part of a window's furniture layout -- every filled
 * rectangle bar the two scrollbar sausages (which track window->scroll and
 * are cheap to recompute each paint) and the title string (font-dependent).
 * wuss__furniture_draw calls this once when the cache is stale, then does a
 * pure clip-and-fill loop over the result. The cache is invalidated on every
 * geometry change through wuss__furniture_invalidate*. */

#include <assert.h>

#include "../core/impl.h"

/* Append one rect/paint-class pair, unless it would overflow the fixed store
 * (WUSS__FURNITURE_MAX_PIECES is sized to the worst case, so this is a guard
 * against a future furniture addition, not an expected path). */
static void push_piece(wuss__furniture_layout_t     *layout,
                       const box_t                  *rect,
                       wuss__furniture_paint_class_t paint)
{
  wuss__furniture_piece_t *piece;

  if (layout->npieces >= WUSS__FURNITURE_MAX_PIECES)
  {
    assert(!"furniture layout piece overflow");
    return;
  }

  piece        = &layout->pieces[layout->npieces++];
  piece->rect  = *rect;
  piece->paint = paint;
}

void wuss__furniture_layout_build(wuss_window_t *window)
{
  wuss__furniture_layout_t *layout;
  box_t                     titlebar;
  box_t                     content;
  point_t                   carve;
  int                       outline_px;

  layout               = &window->furniture_layout;
  layout->npieces      = 0;
  layout->has_titlebar = 0;

  outline_px = wuss__outline_px(window);
  wuss__content_box(window, &content);
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  /* titlebar fill + its icons ------------------------------------------- */
  wuss__titlebar_box(window, &titlebar);
  if (!(window->flags & wuss_WINDOW_NO_TITLEBAR))
  {
    layout->titlebar     = titlebar;
    layout->has_titlebar = 1;

    push_piece(layout, &titlebar, wuss__FURNITURE_PAINT_TITLE_BG);

    if (!(window->flags & wuss_WINDOW_NO_CLOSE))
    {
      box_t close;

      wuss__close_box(window, &close);
      push_piece(layout, &close, wuss__FURNITURE_PAINT_CLOSE);
    }

    if (!(window->flags & wuss_WINDOW_NO_BACK))
    {
      box_t back;

      wuss__back_box(window, &back);
      push_piece(layout, &back, wuss__FURNITURE_PAINT_BACK);
    }

    if (!(window->flags & wuss_WINDOW_NO_TOGGLE_SIZE))
    {
      box_t toggle;

      wuss__toggle_box(window, &toggle);
      push_piece(layout, &toggle, wuss__FURNITURE_PAINT_TOGGLE);
    }
  }

  /* resize-only carve bands ------------------------------------------------
   * Both scrollbars off but a resize icon on: the content box is still
   * carved back on its right and bottom and no scroll strip paints that
   * margin, so furniture fills it (see the note in the old draw.c). */
  if ((window->flags & wuss_WINDOW_NO_VSCROLL) &&
      (window->flags & wuss_WINDOW_NO_HSCROLL) &&
      !(window->flags & wuss_WINDOW_NO_RESIZE))
  {
    box_t band;

    if (carve.x > 0)
    {
      band.x0 = content.x1;
      band.x1 = window->visible.x1 - outline_px;
      band.y0 = window->visible.y0 + outline_px
              + wuss__titlebar_height(window);
      band.y1 = window->visible.y1 - outline_px;
      push_piece(layout, &band, wuss__FURNITURE_PAINT_TITLE_BG);
    }

    if (carve.y > 0)
    {
      band.x0 = window->visible.x0 + outline_px;
      band.x1 = content.x1;
      band.y0 = content.y1;
      band.y1 = window->visible.y1 - outline_px;
      push_piece(layout, &band, wuss__FURNITURE_PAINT_TITLE_BG);
    }
  }

  /* resize icon + its dividing seams ---------------------------------- */
  if (!(window->flags & wuss_WINDOW_NO_RESIZE))
  {
    box_t resize, rule;
    int   top_seam, left_seam;

    wuss__resize_box(window, &resize);

    top_seam  = (window->flags & wuss_WINDOW_NO_VSCROLL) ? 0 : WUSS_DIVIDER_PX;
    left_seam = (window->flags & wuss_WINDOW_NO_HSCROLL) ? 0 : WUSS_DIVIDER_PX;

    push_piece(layout, &resize, wuss__FURNITURE_PAINT_RESIZE);

    if (top_seam > 0)
    {
      rule.x0 = resize.x0 - left_seam;
      rule.x1 = resize.x1;
      rule.y0 = resize.y0 - top_seam;
      rule.y1 = resize.y0;
      push_piece(layout, &rule, wuss__FURNITURE_PAINT_TITLE_BG);
    }

    if (left_seam > 0)
    {
      rule.x0 = resize.x0 - left_seam;
      rule.x1 = resize.x0;
      rule.y0 = resize.y0 - top_seam;
      rule.y1 = resize.y1;
      push_piece(layout, &rule, wuss__FURNITURE_PAINT_TITLE_BG);
    }
  }

  /* vertical scrollbar: arrows + well (sausage is drawn live) --------- */
  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
  {
    box_t up, down, well;

    wuss__vscroll_up_box(window, &up);
    push_piece(layout, &up, wuss__FURNITURE_PAINT_SCROLL_ARROWS);

    wuss__vscroll_down_box(window, &down);
    push_piece(layout, &down, wuss__FURNITURE_PAINT_SCROLL_ARROWS);

    wuss__vscroll_well_box(window, &well);
    push_piece(layout, &well, wuss__FURNITURE_PAINT_SCROLL_WELLS);
  }

  /* horizontal scrollbar: arrows + well ----------------------------- */
  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    box_t left, right, well;

    wuss__hscroll_left_box(window, &left);
    push_piece(layout, &left, wuss__FURNITURE_PAINT_SCROLL_ARROWS);

    wuss__hscroll_right_box(window, &right);
    push_piece(layout, &right, wuss__FURNITURE_PAINT_SCROLL_ARROWS);

    wuss__hscroll_well_box(window, &well);
    push_piece(layout, &well, wuss__FURNITURE_PAINT_SCROLL_WELLS);
  }

  /* interior rules where furniture is carved off the content edges --- */
  if (carve.x > 0)
  {
    box_t rule;

    rule.x0 = content.x1;
    rule.x1 = content.x1 + WUSS_DIVIDER_PX;
    rule.y0 = content.y0;
    rule.y1 = content.y1;
    push_piece(layout, &rule, wuss__FURNITURE_PAINT_TITLE_BG);
  }

  if (carve.y > 0)
  {
    box_t rule;

    rule.x0 = content.x0;
    rule.x1 = content.x1 + ((carve.x > 0) ? WUSS_DIVIDER_PX : 0); /* meet the vertical rule at the corner */
    rule.y0 = content.y1;
    rule.y1 = content.y1 + WUSS_DIVIDER_PX;
    push_piece(layout, &rule, wuss__FURNITURE_PAINT_TITLE_BG);
  }

  /* window outline: four one-pixel edges ---------------------------- */
  if (!(window->flags & wuss_WINDOW_NO_OUTLINE))
  {
    box_t edge;

    edge.x0 = window->visible.x0;
    edge.y0 = window->visible.y0;
    edge.x1 = window->visible.x1;
    edge.y1 = window->visible.y0 + 1;
    push_piece(layout, &edge, wuss__FURNITURE_PAINT_OUTLINE); /* top */

    edge.y0 = window->visible.y1 - 1;
    edge.y1 = window->visible.y1;
    push_piece(layout, &edge, wuss__FURNITURE_PAINT_OUTLINE); /* bottom */

    edge.x0 = window->visible.x0;
    edge.y0 = window->visible.y0;
    edge.x1 = window->visible.x0 + 1;
    edge.y1 = window->visible.y1;
    push_piece(layout, &edge, wuss__FURNITURE_PAINT_OUTLINE); /* left */

    edge.x0 = window->visible.x1 - 1;
    edge.x1 = window->visible.x1;
    push_piece(layout, &edge, wuss__FURNITURE_PAINT_OUTLINE); /* right */
  }

  layout->valid = 1;
}
