/* wuss/iconbar.h -- RISC OS-style icon bar, internal */

#ifndef WUSS_ICONBAR_IMPL_H
#define WUSS_ICONBAR_IMPL_H

#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/screen.h"

#include "wuss/wuss.h"
#include "wuss/iconbar.h"

/* Slot geometry: an icon is as wide as its label (or image, whichever is
 * wider; WUSS_ICONBAR_ICON if it has neither) and packs from its side's edge
 * in creation order, with WUSS_ICONBAR_GAP of empty bar between neighbours
 * and at the edge -- the gap is not drawn on, so it reads as space rather
 * than a shared border. An icon bar wider than the screen scrolls (see
 * WUSS_ICONBAR_SCROLL_ZONE). */
#define WUSS_ICONBAR_ICON       34
#define WUSS_ICONBAR_GAP        8
#define WUSS_ICONBAR_HEIGHT     67
#define WUSS_ICONBAR_TEXT_PAD   2  /* clear space above the label */

/* An icon bar wider than the screen scrolls: while the pointer is within
 * WUSS_ICONBAR_SCROLL_ZONE pixels of either end of the bar, each idle moves
 * the icons WUSS_ICONBAR_SCROLL_STEP pixels. */
#define WUSS_ICONBAR_SCROLL_ZONE 40
#define WUSS_ICONBAR_SCROLL_STEP 6

/* Vertical placement: the icon (plus WUSS_ICONBAR_TEXT_PAD and the font
 * height if labelled) has the spare bar height split 5:3 above:below. */
#define WUSS_ICONBAR_OUTLINE    1

/* A live icon bar icon is its creation spec plus the task it delivers
 * wuss_EVENT_ICON to. spec.text is owned (strdup'd by
 * wuss_iconbar_icon_create); never NULL, "" instead. */
struct wuss_iconbar_icon
{
  wuss_iconbar_icon_spec_t  spec;  /* text owned */
  wuss_task_t              *owner; /* wuss_EVENT_ICON delivery target */
};

/* The bar's on-screen box: wuss->iconbar.window's own visible box, or an
 * empty box if the bar has no window yet (before the first icon is
 * created). */
void wuss__iconbar_box(const wuss_t *wuss, box_t *out);

/* Icon i's on-screen slot box within the bar (screen space): its full height
 * and width, excluding the gap beside it. Does not clip to the bar's own box
 * -- a slot off the screen is the caller's to skip drawing/hit-testing. */
void wuss__iconbar_slot_box(const wuss_t *wuss, int index, box_t *out);

/* Icon i's own box (screen space): as wide as its slot, as tall as its
 * image, placed as above. */
void wuss__iconbar_icon_box(const wuss_t *wuss, int index, box_t *out);

/* Idle step: scroll the bar while the pointer hovers near an end of an
 * overfull bar. Called from wuss_idle. */
void wuss__iconbar_scroll_tick(wuss_t *wuss);

/* Paint every icon (image/label) into "piece", a piece of
 * wuss->iconbar.window's content already backdrop-filled and clipped by
 * redraw_window. Called from redraw_window for wuss->iconbar.window, same as
 * any other window's icon-drawing step. */
void wuss__iconbar_draw_icons(wuss_t *wuss, const box_t *piece);

/* Hit-test the bar against a screen-space point. Returns the icon under it,
 * or NULL if the point is outside the bar or over empty slot space. */
wuss_iconbar_icon_t *wuss__iconbar_hit_test(wuss_t *wuss, point_t p);

/* Click/drag dispatch for wuss_mouse_click, called once the point has
 * already resolved to wuss->iconbar.window: see iconbar/hit-test.c. Returns
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
