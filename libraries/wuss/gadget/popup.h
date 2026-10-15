/* wuss/gadget/popup.h -- layout and menu placement shared by the pop-up field gadgets */

#ifndef WUSS_GADGET_POPUP_H
#define WUSS_GADGET_POPUP_H

#include "base/result.h"
#include "geom/box.h"
#include "geom/point.h"

#include "wuss/icon-spec.h"
#include "wuss/window.h"

#include "../core/types.h"

/* Fill "spec" as the arrow at the right edge of "bbox", vertically centred,
 * from the icon pool bitmaps "name" and "pressed_name"; a square ">" button
 * the height of "bbox" when the pool lacks "name". Returns its width. */
int wuss__popup_arrow_spec(const wuss_t     *wuss,
                           box_t             bbox,
                           const char       *name,
                           const char       *pressed_name,
                           wuss_icon_spec_t *spec);

/* Lay out a title, field and arrow within "bbox". Fills specs[1] with the
 * arrow and, when there is a title icon, specs[2] with the title; sets
 * "field" to the box left for the field icon, which the caller makes
 * specs[0]. "title_width" is the title's width, 0 to fit its text, negative
 * for no title icon. Returns the number of specs used (2 or 3). */
int wuss__popup_layout(const wuss_t    *wuss,
                       box_t            bbox,
                       const char      *title,
                       int              title_width,
                       const char      *arrow_name,
                       const char      *arrow_pressed_name,
                       wuss_icon_spec_t specs[3],
                       box_t           *field);

/* Open "menu" with its top-left at the top-right of "arrow" in "window". */
result_t wuss__popup_open(wuss_window_t      *window,
                          wuss_icon_t        *arrow,
                          const wuss_menu_t  *menu,
                          wuss_menu_handle_t *handle);

#endif /* WUSS_GADGET_POPUP_H */
