/* furniture.h -- wuss - minimal window manager */

#ifndef WUSS_FURNITURE_IMPL_H
#define WUSS_FURNITURE_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "wuss/wuss.h"

#define WUSS_MIN_SAUSAGE 6  /* scrollbar sausage never shrinks below this, however small the content fraction */
#define WUSS_SCROLL_STEP 20 /* pixels stepped per scrollbar arrow click */

/* Which region of a window's border (or its content) a point falls in. */
typedef enum wuss_furniture_region
{
  wuss_FURNITURE_NONE,
  wuss_FURNITURE_CONTENT,
  wuss_FURNITURE_BACK,
  wuss_FURNITURE_CLOSE,
  wuss_FURNITURE_TITLE,
  wuss_FURNITURE_TOGGLE_SIZE,
  wuss_FURNITURE_VSCROLL_UP,
  wuss_FURNITURE_VSCROLL_WELL,
  wuss_FURNITURE_VSCROLL_DOWN,
  wuss_FURNITURE_RESIZE,
  wuss_FURNITURE_HSCROLL_RIGHT,
  wuss_FURNITURE_HSCROLL_WELL,
  wuss_FURNITURE_HSCROLL_LEFT
}
wuss_furniture_region_t;

/* Which kind of drag a furniture region starts, if any. */
typedef enum wuss_furniture_drag_kind
{
  wuss_FURNITURE_DRAG_NONE,
  wuss_FURNITURE_DRAG_MOVE,
  wuss_FURNITURE_DRAG_RESIZE,
  wuss_FURNITURE_DRAG_VSCROLL_SAUSAGE,
  wuss_FURNITURE_DRAG_HSCROLL_SAUSAGE
}
wuss_furniture_drag_kind_t;

/* Furniture drag state, embedded in struct wuss. */
struct wuss__furniture
{
  wuss_window_t              *dragging;  /* NULL when idle */
  wuss_furniture_drag_kind_t  drag_kind;
  point_t                     drag; /* MOVE: pointer offset within content;
                                     * RESIZE: unused, recomputed each move;
                                     * *_SAUSAGE: pointer position at drag start */
  int                         drag_scroll_start; /* *_SAUSAGE: scroll.x/scroll.y at drag start */
};

/* Furniture chrome colours, one entry per class of furniture, embedded in
 * struct wuss. Title is the only two-tone class (fill + text); the rest
 * are drawn as a single flat colour. */
struct wuss__furniture_colours
{
  struct
  {
    wuss_colour_t bg;
    wuss_colour_t fg;
  }
  title;
  wuss_colour_t back;
  wuss_colour_t close;
  wuss_colour_t toggle;
  wuss_colour_t resize;
  wuss_colour_t arrows;
  wuss_colour_t wells;
  wuss_colour_t sausages;
};

static inline wuss_furniture_drag_kind_t wuss__furniture_drag_kind(wuss_furniture_region_t region)
{
  switch (region)
  {
  case wuss_FURNITURE_TITLE:
  case wuss_FURNITURE_CLOSE:
    return wuss_FURNITURE_DRAG_MOVE;
  case wuss_FURNITURE_RESIZE:
    return wuss_FURNITURE_DRAG_RESIZE;
  case wuss_FURNITURE_VSCROLL_WELL:
    return wuss_FURNITURE_DRAG_VSCROLL_SAUSAGE;
  case wuss_FURNITURE_HSCROLL_WELL:
    return wuss_FURNITURE_DRAG_HSCROLL_SAUSAGE;
  default:
    return wuss_FURNITURE_DRAG_NONE;
  }
}

wuss_furniture_region_t wuss__furniture_hit_test(const wuss_window_t *window,
                                                 point_t              p);
void wuss__furniture_draw(wuss_t        *wuss,
                          wuss_window_t *window,
                          const box_t   *full);
void wuss__furniture_invalidate(wuss_window_t *window);
void wuss__furniture_invalidate_for(wuss_window_t *window, const box_t *visible);

/* geometry: titlebar icons */
void wuss__back_box(const wuss_window_t *window, box_t *out);
void wuss__toggle_box(const wuss_window_t *window, box_t *out);
void wuss__resize_box(const wuss_window_t *window, box_t *out);

/* geometry: vertical scrollbar */
void wuss__vscroll_up_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_down_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_well_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_sausage_box(const wuss_window_t *window, box_t *out);
int  wuss__vscroll_well_px(const wuss_window_t *window);

/* geometry: horizontal scrollbar */
void wuss__hscroll_left_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_right_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_well_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_sausage_box(const wuss_window_t *window, box_t *out);
int  wuss__hscroll_well_px(const wuss_window_t *window);

/* actions */
void wuss__furniture_toggle_size(wuss_window_t *window);
void wuss__furniture_scroll_step(wuss_window_t *window, point_t delta);
point_t wuss__scroll_clamp(const wuss_window_t *window, point_t desired);
void wuss__furniture_drag_resize(wuss_window_t *window, point_t p);
void wuss__furniture_drag_sausage(wuss_window_t *window,
                                  int            delta_px,
                                  int            scroll_start,
                                  int            horizontal);

#endif /* WUSS_FURNITURE_IMPL_H */
