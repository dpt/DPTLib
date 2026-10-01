/* wuss/iconbar.h -- RISC OS-style icon bar, internal */

#ifndef WUSS_ICONBAR_IMPL_H
#define WUSS_ICONBAR_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/iconbar.h"

/* Fixed slot geometry: each icon gets a square this many pixels on a side
 * (WUSS_ICONBAR_ICON), left to right with WUSS_ICONBAR_GAP of empty bar
 * between them -- unlike the icon square itself, the gap is not drawn on,
 * so it reads as space rather than a shared border. An icon bar wider than
 * the screen just clips -- no wrap, no scroll. */
#define WUSS_ICONBAR_ICON       34
#define WUSS_ICONBAR_GAP        8
#define WUSS_ICONBAR_SLOT       (WUSS_ICONBAR_ICON + WUSS_ICONBAR_GAP)
#define WUSS_ICONBAR_HEIGHT     66
#define WUSS_ICONBAR_BEVEL      2  /* matches screen_draw_bevel_edge's fixed
                                    * 2px edge */
#define WUSS_ICONBAR_TEXT_PAD   2  /* clear space above the label */

/* Vertical placement of the icon square within the bar, measured inside its
 * 1px outline: 20px spare above, the icon, 10px spare below -- 1 + 20 + 34 +
 * 10 + 1 == WUSS_ICONBAR_HEIGHT. */
#define WUSS_ICONBAR_OUTLINE    1
#define WUSS_ICONBAR_TOP_SPARE  20
#define WUSS_ICONBAR_BOT_SPARE  10

/* Per-instance transient state, kept as bitflags with wuss__iconbar_icon_*
 * accessors (mirrors wuss_icon_state_t) so more can be added without
 * growing the struct. */
typedef enum wuss_iconbar_icon_state
{
  wuss_ICONBAR_ICON_STATE_NONE    = 0,
  wuss_ICONBAR_ICON_STATE_PRESSED = 1 << 0 /* held with the pointer inside */
}
wuss_iconbar_icon_state_t;

/* A live icon bar icon is its creation spec plus the task it delivers
 * wuss_EVENT_ICON to and its transient runtime state. spec.text is owned
 * (strdup'd by wuss_iconbar_icon_create); never NULL, "" instead. */
struct wuss_iconbar_icon
{
  wuss_iconbar_icon_spec_t  spec;  /* text owned */
  wuss_task_t              *owner; /* wuss_EVENT_ICON delivery target */
  wuss_iconbar_icon_state_t state;
};

static inline int wuss__iconbar_icon_pressed(const wuss_iconbar_icon_t *icon)
{
  return (icon->state & wuss_ICONBAR_ICON_STATE_PRESSED) != 0;
}

/* The bar's on-screen box: wuss->iconbar_window's own visible box, or an
 * empty box if the bar has no window yet (before the first icon is
 * created). */
void wuss__iconbar_box(const wuss_t *wuss, box_t *out);

/* Icon i's on-screen slot box within the bar (screen space), left to right
 * in creation order. Does not clip to the bar's own box -- a slot past the
 * screen's right edge is the caller's to skip drawing/hit-testing. */
void wuss__iconbar_slot_box(const wuss_t *wuss, int index, box_t *out);

/* Icon i's own WUSS_ICONBAR_ICON-square box (screen space): the slot box
 * narrowed to the icon's actual width and placed per WUSS_ICONBAR_TOP_SPARE/
 * WUSS_ICONBAR_BOT_SPARE, excluding the gap that follows it and the bar's
 * outline. */
void wuss__iconbar_icon_box(const wuss_t *wuss, int index, box_t *out);

/* Paint every icon (bevel/image/label) into "piece", a piece of
 * wuss->iconbar_window's content already backdrop-filled and clipped by
 * redraw_window. Called from redraw_window for wuss->iconbar_window, same as
 * any other window's icon-drawing step. */
void wuss__iconbar_draw_icons(wuss_t *wuss, const box_t *piece);

/* Hit-test the bar against a screen-space point. Returns the icon under it,
 * or NULL if the point is outside the bar or over empty slot space. */
wuss_iconbar_icon_t *wuss__iconbar_hit_test(wuss_t *wuss, point_t p);

/* Click/drag dispatch for wuss_mouse_click, called once the point has
 * already resolved to wuss->iconbar_window: see iconbar/hit-test.c. Returns
 * 1 when an icon claimed the event, 0 for empty slot space (caller still
 * treats the click as handled -- the bar's window owns anything within its
 * own box). */
int wuss__iconbar_icon_click(wuss_t             *wuss,
                             point_t             p,
                             wuss_button_t       button,
                             wuss_mouse_action_t action);

/* Remove every icon owned by "task" from the bar, invalidating their slots.
 * Called by wuss_task_destroy/wuss_destroy before the task is freed. No-op
 * if the bar has no icons owned by it. */
void wuss__iconbar_task_destroyed(wuss_t *wuss, wuss_task_t *task);

/* Free the bar's whole icon store. Teardown only: does not invalidate. */
void wuss__iconbar_free(wuss_t *wuss);

#endif /* WUSS_ICONBAR_IMPL_H */
