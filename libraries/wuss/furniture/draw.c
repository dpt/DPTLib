/* wuss/furniture/draw.c -- wuss - minimal window manager */

/* Two-phase furniture paint. Phase 1 (wuss__furniture_layout_build, cached)
 * computes every filled rect from the window geometry. Phase 2 (here) walks
 * the cache clipping each rect to the redraw region "full" and filling it.
 * The two scrollbar sausages track window->scroll so they are recomputed
 * each paint rather than cached; the title string is font-dependent and
 * drawn live. */

#include <assert.h>
#include <string.h>

#include "base/utils.h"
#include "geom/point.h"

#include "../core/impl.h"

/* Paint one furniture rectangle "b" in "colour", clipped to the part of it
 * that falls inside "full" (the redraw region). A no-op when "b" is wholly
 * outside "full". Pins scr->clip to the clipped rect -- the caller's next
 * draw is expected to set its own clip. */
static void fill_furniture_rect(wuss_t      *wuss,
                                const box_t *b,
                                const box_t *full,
                                colour_t     colour)
{
  box_t clipped;

  if (box_intersection(b, full, &clipped))
    return; /* wholly outside the redraw region */

  wuss->scr->clip = clipped;
  screen_fill_rect(wuss->scr, b->x0, b->y0, box_size(b), colour);
}

/* Resolve a cached piece's paint class to a concrete palette colour. */
static colour_t paint_colour(const wuss_t                 *wuss,
                             wuss__furniture_paint_class_t paint)
{
  const wuss_furniture_palette_t *fc;

  fc = &wuss->furniture_colours;

  switch (paint)
  {
  case wuss__FURNITURE_PAINT_TITLE_BG:       return wuss->palette[fc->title.bg];
  case wuss__FURNITURE_PAINT_CLOSE:          return wuss->palette[fc->close];
  case wuss__FURNITURE_PAINT_BACK:           return wuss->palette[fc->back];
  case wuss__FURNITURE_PAINT_TOGGLE:         return wuss->palette[fc->toggle];
  case wuss__FURNITURE_PAINT_RESIZE:         return wuss->palette[fc->resize];
  case wuss__FURNITURE_PAINT_SCROLL_ARROWS:  return wuss->palette[fc->scroll.arrows];
  case wuss__FURNITURE_PAINT_SCROLL_WELLS:   return wuss->palette[fc->scroll.wells];
  case wuss__FURNITURE_PAINT_OUTLINE:        return wuss->palette[fc->outline];
  }

  assert(!"unhandled furniture paint class");
  return wuss->palette[fc->title.bg];
}

/* The title string, drawn into its titlebar slot. Split out of the main
 * routine only to keep the phase-2 loop readable; still runs every paint
 * because measurement depends on the current font. */
static void draw_title(wuss_t        *wuss,
                       wuss_window_t *window,
                       const box_t   *titlebar,
                       const box_t   *titlebar_clip)
{
  bmfont_t      *titlefont;
  point_t        pos;
  int            text_x0, text_x1, titlelen, split_point, ascent;
  bmfont_width_t width;
  box_t          text_box, text_clip;

  if (window->title[0] == '\0')
    return;

  /* window titles are drawn in the bold weight (font slot 1) when one was
   * supplied, falling back to the system font */
  titlefont = (wuss->fonts.nfonts > 1 && wuss->fonts.fonts[1] != NULL)
            ? wuss->fonts.fonts[1]
            : wuss->fonts.fonts[0];
  if (titlefont == NULL)
    return;

  text_x0 = titlebar->x0 + 2;
  if (window->flags & wuss_WINDOW_CLOSE)
  {
    box_t close;

    wuss__close_box(window, &close);
    text_x0 = close.x1 + 2;
  }
  else if (window->flags & wuss_WINDOW_BACK)
  {
    box_t back;

    wuss__back_box(window, &back);
    text_x0 = back.x1 + 2;
  }

  text_x1 = titlebar->x1 - 2;
  if (window->flags & wuss_WINDOW_TOGGLE_SIZE)
  {
    box_t toggle;

    wuss__toggle_box(window, &toggle);
    text_x1 = toggle.x0 - 2;
  }

  /* A title too wide for its slot mustn't bleed into a neighbouring icon:
   * clip drawing to the slot itself, not just the whole titlebar, so a
   * too-long title is cut off cleanly rather than overdrawing whatever
   * furniture the current redraw didn't happen to touch. */
  text_box.x0 = text_x0;
  text_box.y0 = titlebar->y0;
  text_box.x1 = text_x1;
  text_box.y1 = titlebar->y1;
  if (text_x1 <= text_x0 || box_intersection(&text_box, titlebar_clip, &text_clip))
    return;

  titlelen = (int) strlen(window->title);
  wuss__text_measure(titlefont, window->title, titlelen, text_x1 - text_x0, &split_point, &width);

  bmfont_get_info(titlefont, NULL, NULL, &ascent, NULL);

  pos.x = (split_point < titlelen) ? text_x0 : text_x0 + MAX(0, ((text_x1 - text_x0) - width) / 2);
  pos.y = titlebar->y0 + 2 + ascent;
  wuss->scr->clip = text_clip;
  wuss__text_draw(titlefont, wuss->scr, window->title, titlelen,
                  wuss->palette[wuss->furniture_colours.title.fg],
                  wuss->palette[wuss->furniture_colours.title.bg],
                  &pos, NULL);
}

void wuss__furniture_draw(wuss_t        *wuss,
                          wuss_window_t *window,
                          const box_t   *full)
{
  box_t visible_clipped;
  box_t sausage;
  int   i;

  if (box_intersection(&window->visible, full, &visible_clipped))
    return; /* offscreen */

  if (!(window->furniture_layout.flags & wuss_FURNITURE_LAYOUT__VALID))
    wuss__furniture_layout_build(window);

  /* phase 2: clip-and-fill every cached rect */
  for (i = 0; i < window->furniture_layout.npieces; i++)
  {
    const wuss__furniture_piece_t *piece;

    piece = &window->furniture_layout.pieces[i];
    fill_furniture_rect(wuss, &piece->rect, full,
                        paint_colour(wuss, piece->paint));
  }

  /* the two scrollbar sausages: geometry depends on window->scroll, so they
   * are computed live rather than cached */
  if (window->flags & wuss_WINDOW_VSCROLL)
  {
    wuss__vscroll_sausage_box(window, &sausage);
    fill_furniture_rect(wuss, &sausage, full,
                        wuss->palette[wuss->furniture_colours.scroll.sausages]);
  }

  if (window->flags & wuss_WINDOW_HSCROLL)
  {
    wuss__hscroll_sausage_box(window, &sausage);
    fill_furniture_rect(wuss, &sausage, full,
                        wuss->palette[wuss->furniture_colours.scroll.sausages]);
  }

  /* the title string, drawn live over its (already-filled) titlebar slot */
  if (window->furniture_layout.flags & wuss_FURNITURE_LAYOUT__HAS_TITLEBAR)
  {
    box_t titlebar_clip;

    if (!box_intersection(&window->furniture_layout.titlebar, full, &titlebar_clip))
      draw_title(wuss, window, &window->furniture_layout.titlebar, &titlebar_clip);
  }
}
