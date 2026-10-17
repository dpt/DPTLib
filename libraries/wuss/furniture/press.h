/* wuss/furniture/press.h -- wuss - furniture click handlers
 *
 * Private to furniture/. Each wuss__furniture_press_* is the press handler
 * (wuss__furniture_press_fn_t) of one or more rows of wuss__furniture_elements;
 * the helpers below are what they share with click.c. */

#ifndef WUSS_FURNITURE_PRESS_H
#define WUSS_FURNITURE_PRESS_H

#include "base/utils.h"

#include "../core/types.h"

/* Move the held-down highlight to "region" (NONE to clear it), repainting the
 * drawn box of whichever region it leaves and whichever it lands on. */
void wuss__furniture_set_pressed(wuss_window_t          *window,
                                 wuss_furniture_region_t region);

/* Hold down a furniture icon that starts no drag: arm "dragging" purely so
 * the MOUSE_UP release clears the highlight. No auto-repeat unless the caller
 * sets one afterwards. */
void wuss__furniture_hold(wuss_window_t          *window,
                          wuss_furniture_region_t region);

/* Arm a drag of "kind" from screen point "p". Records the scroll offset and
 * whether Adjust is held, for the sausage drags. */
void wuss__furniture_arm_drag(wuss_window_t             *window,
                              wuss_furniture_drag_kind_t kind,
                              point_t                    p,
                              wuss_button_t              button);

/* Icons (furniture/press-icons.c). */
wuss__furniture_press_fn_t wuss__furniture_press_close;
wuss__furniture_press_fn_t wuss__furniture_press_back;
wuss__furniture_press_fn_t wuss__furniture_press_toggle;

/* Scroll arrows and wells (furniture/press-scroll.c). */
wuss__furniture_press_fn_t wuss__furniture_press_arrow;
wuss__furniture_press_fn_t wuss__furniture_press_well;

/* Titlebar and resize drags (furniture/press-drag.c). */
wuss__furniture_press_fn_t wuss__furniture_press_title;
wuss__furniture_press_fn_t wuss__furniture_press_resize;

#endif /* WUSS_FURNITURE_PRESS_H */
