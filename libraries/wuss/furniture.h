/* wuss/furniture.h -- wuss - minimal window manager */

#ifndef WUSS_FURNITURE_IMPL_H
#define WUSS_FURNITURE_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "wuss/wuss.h"

#define WUSS_MIN_SAUSAGE    1  /* scrollbar sausage never shrinks below this, however small the content fraction */
#define WUSS_SCROLL_END_GAP 2  /* sausage along-axis margin from its well's ends, purely cosmetic */
#define WUSS_SCROLL_STEP    20 /* pixels stepped per scrollbar arrow click */

/* Cached furniture layout ------------------------------------------------- */

/* Which chrome colour a cached furniture rect is painted in. The layout
 * stores this class rather than a resolved colour so a palette change needs
 * no layout rebuild: wuss__furniture_draw resolves it through wuss->palette
 * at paint time. */
typedef enum wuss__furniture_paint_class
{
  wuss__FURNITURE_PAINT_TITLE_BG,   /* titlebar fill, resize-carve bands */
  wuss__FURNITURE_PAINT_CLOSE,
  wuss__FURNITURE_PAINT_BACK,
  wuss__FURNITURE_PAINT_TOGGLE,
  wuss__FURNITURE_PAINT_RESIZE,
  wuss__FURNITURE_PAINT_SCROLL_ARROWS,
  wuss__FURNITURE_PAINT_SCROLL_WELLS,
  wuss__FURNITURE_PAINT_OUTLINE
}
wuss__furniture_paint_class_t;

/* One filled rectangle in the cached layout. */
typedef struct wuss__furniture_piece
{
  box_t                         rect;
  wuss__furniture_paint_class_t  paint;
}
wuss__furniture_piece_t;

/* Upper bound on pieces a single window's furniture can contribute:
 * titlebar(1) + close/back/toggle(3) + no-scroll resize bands(2) +
 * resize icon + its two seams(3) + v/h scroll arrows+well(3+3) +
 * two interior rules(2) + four outline edges(4) = 21. Round up. */
#define WUSS__FURNITURE_MAX_PIECES 24

/* Internal wuss__furniture_layout::flags bits. */
enum
{
  /* the cache is up to date. Rebuilt lazily by wuss__furniture_draw when
   * clear; wuss__furniture_invalidate* clear it on any geometry change. */
  wuss_FURNITURE_LAYOUT__VALID       = 1u << 0,

  /* `titlebar` holds a real rect for the live title-text pass */
  wuss_FURNITURE_LAYOUT__HAS_TITLEBAR = 1u << 1
};

/* Per-window cache of the furniture layout: every filled rect except the two
 * scrollbar sausages (which move with window->scroll and are recomputed each
 * paint) and the title text (font-dependent, drawn live). */
typedef struct wuss__furniture_layout
{
  unsigned int            flags;
  int                     npieces;
  wuss__furniture_piece_t pieces[WUSS__FURNITURE_MAX_PIECES];
  box_t                   titlebar;   /* for the live title-text pass; empty if no titlebar */
}
wuss__furniture_layout_t;

/* Populate window->furniture_layout from the current geometry and flags. */
void wuss__furniture_layout_build(wuss_window_t *window);

/* Shift an already-valid window->furniture_layout by (dx, dy) in place --
 * for a pure window move, where every cached rect stays correct relative to
 * the window, just offset in screen space, so there is no need to drop the
 * cache and rebuild all pieces from scratch. */
void wuss__furniture_layout_translate(wuss_window_t *window, int dx, int dy);

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
  wuss_furniture_region_t     pressed_region; /* RESIZE or a scroll arrow while
                                               * held down, drawn in
                                               * button_pressed; NONE
                                               * otherwise. Only "dragging"'s
                                               * window is ever pressed. */
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

/* The drawn (un-grown) box for a pressable region -- RESIZE or one of the
 * four scroll arrows -- used to invalidate/highlight it while held. "region"
 * must be one of those five; any other value is a caller error. */
void wuss__furniture_pressed_box(const wuss_window_t    *window,
                                 wuss_furniture_region_t region,
                                 box_t                  *out);
void wuss__furniture_draw(wuss_t        *wuss,
                          wuss_window_t *window,
                          const box_t   *full);
void wuss__furniture_invalidate(wuss_window_t *window);
void wuss__furniture_invalidate_for(wuss_window_t *window,
                                    const box_t   *visible);

/* Dispatch table the wuss core reaches window furniture through: everything
 * the core (z-order, mouse routing, redraw, window lifecycle) calls that
 * lives in the furniture .c files. A caller could point wuss::furniture_ops at its
 * own table to replace the furniture wholesale; wuss_create defaults it to
 * wuss__furniture_default_ops. The geometry helpers in impl.h
 * (wuss__titlebar_box, wuss__content_box, wuss__furniture_carve_for, the
 * titlebar-height/button-size inlines) are a compile-time seam between the
 * furniture and no-furniture builds and stay out of this table. */
typedef struct wuss__furniture_ops
{
  wuss_furniture_region_t (*hit_test)(const wuss_window_t *window, point_t p);
  void (*draw)(wuss_t *wuss, wuss_window_t *window, const box_t *full);
  void (*invalidate)(wuss_window_t *window);
  void (*invalidate_for)(wuss_window_t *window, const box_t *visible);
  void (*toggle_size)(wuss_window_t *window);
  void (*drag_resize)(wuss_window_t *window, point_t p);
  void (*drag_sausage)(wuss_window_t *window,
                       int            delta_px,
                       int            scroll_start,
                       int            horizontal);
}
wuss__furniture_ops_t;

extern const wuss__furniture_ops_t wuss__furniture_default_ops;

/* geometry: titlebar icons. The _box helpers are the drawn rectangles; the
 * _hit_box helpers are the same rectangles grown outward to tile the outline
 * band and the window corners for hit testing (see furniture/hit-test.c). */
void wuss__back_box(const wuss_window_t *window, box_t *out);
void wuss__back_hit_box(const wuss_window_t *window, box_t *out);
void wuss__toggle_box(const wuss_window_t *window, box_t *out);
void wuss__toggle_hit_box(const wuss_window_t *window, box_t *out);
void wuss__resize_box(const wuss_window_t *window, box_t *out);
void wuss__resize_hit_box(const wuss_window_t *window, box_t *out);

/* geometry: vertical scrollbar */
void wuss__vscroll_up_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_up_hit_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_down_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_down_hit_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_well_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_well_hit_box(const wuss_window_t *window, box_t *out);
void wuss__vscroll_sausage_box(const wuss_window_t *window, box_t *out);
int  wuss__vscroll_well_px(const wuss_window_t *window);

/* geometry: horizontal scrollbar */
void wuss__hscroll_left_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_left_hit_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_right_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_right_hit_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_well_box(const wuss_window_t *window, box_t *out);
void wuss__hscroll_well_hit_box(const wuss_window_t *window, box_t *out);
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
