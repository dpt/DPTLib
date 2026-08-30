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
  point_t                     drag; /* *_SAUSAGE: pointer position at drag start;
                                       * MOVE: pointer offset within content */
  point_t                     drag_offset; /* RESIZE: pointer offset from the
                                            * content box's bottom-right corner
                                            * at drag start, so the grab point
                                            * stays under the pointer instead
                                            * of the window's edge snapping to
                                            * it on the first move */
  int                         drag_scroll_start; /* *_SAUSAGE: scroll.x/scroll.y at drag start */
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
void wuss__furniture_invalidate_for(wuss_window_t *window,
                                    const box_t   *visible);

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
void wuss__furniture_drag_resize(wuss_window_t *window, point_t p);
void wuss__furniture_drag_sausage(wuss_window_t *window,
                                  int            delta_px,
                                  int            scroll_start,
                                  int            horizontal);

#endif /* WUSS_FURNITURE_IMPL_H */
