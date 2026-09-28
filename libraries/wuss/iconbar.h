/* wuss/iconbar.h -- RISC OS-style icon bar, internal */

#ifndef WUSS_ICONBAR_IMPL_H
#define WUSS_ICONBAR_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/iconbar.h"

/* Fixed slot geometry: each icon gets a square slot this many pixels on a
 * side, laid out left to right with no gap between slots (the bevel border
 * drawn inside each slot reads as the gap, as on a real RISC OS icon bar).
 * An icon bar wider than the screen just clips -- no wrap, no scroll. */
#define WUSS_ICONBAR_SLOT       68
#define WUSS_ICONBAR_HEIGHT     68
#define WUSS_ICONBAR_BEVEL      2  /* matches screen_draw_bevel_edge's fixed
                                    * 2px edge */
#define WUSS_ICONBAR_TEXT_PAD   2  /* clear space above the label */

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

/* The bar's on-screen box: full screen width, WUSS_ICONBAR_HEIGHT tall,
 * pinned to the bottom edge. Recomputed from the current screen size on
 * every call, so a screen resize needs no separate bar-geometry update. */
void wuss__iconbar_box(const wuss_t *wuss, box_t *out);

/* Icon i's on-screen slot box within the bar (screen space), left to right
 * in creation order. Does not clip to the bar's own box -- a slot past the
 * screen's right edge is the caller's to skip drawing/hit-testing. */
void wuss__iconbar_slot_box(const wuss_t *wuss, int index, box_t *out);

/* Paint the whole bar -- its background strip and every icon in its slot --
 * onto wuss->scr, clipped to whatever of the bar's box "clip" (screen
 * space) covers. Called last in a redraw pass so the bar always draws on
 * top of every window. */
void wuss__iconbar_draw(wuss_t *wuss, const box_t *clip);

/* Hit-test the bar against a screen-space point. Returns the icon under it,
 * or NULL if the point is outside the bar or over empty slot space. */
wuss_iconbar_icon_t *wuss__iconbar_hit_test(wuss_t *wuss, point_t p);

/* Click/drag dispatch for wuss_mouse_click; see iconbar/hit-test.c. Returns
 * 1 when the bar claimed the event, 0 to fall through to window dispatch. */
int wuss__iconbar_mouse_click(wuss_t             *wuss,
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
