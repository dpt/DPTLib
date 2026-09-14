/* wuss/icon-spec.h -- fill helpers for common wuss_icon_spec_t shapes */

/**
 * \file icon-spec.h
 *
 * A task laying out a dialogue with wuss_icon_create_array typically builds
 * several wuss_icon_spec_t entries that only differ in bbox, text and a
 * couple of type-specific fields. These helpers fill one spec in place for
 * the three shapes that recur across the test tasks: a plain or
 * right-justified label, a horizontal or vertical slider, and an action
 * button (optionally the dialogue's default action). Each takes the spec to
 * fill plus its distinguishing fields; the caller is still responsible for
 * memset(specs, 0, sizeof(specs)) up front and for wuss_icon_create_array.
 */

#ifndef WUSS_ICON_SPEC_H
#define WUSS_ICON_SPEC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "geom/box.h"

#include "wuss/icon.h"

#ifdef WUSS_ICONS

/* ----------------------------------------------------------------------- */

/**
 * Fill \p spec as a wuss_ICON_TYPE_LABEL.
 *
 * \param[out] spec        Spec to fill; any prior contents are overwritten.
 * \param[in]  bbox         Bounding box.
 * \param[in]  text         Label text, borrowed until
 *                          wuss_icon_create(_array) copies it.
 * \param[in]  fg           Text colour.
 * \param[in]  justify_right Non-zero to right-justify
 *                           (wuss_ICON_FLAGS_JUSTIFY_RIGHT) instead of the
 *                           default left justification.
 */
void wuss_icon_spec_label(wuss_icon_spec_t *spec,
                          box_t             bbox,
                          const char       *text,
                          wuss_colour_t     fg,
                          int               justify_right);

/**
 * Fill \p spec as a wuss_ICON_TYPE_SLIDER.
 *
 * \param[out] spec          Spec to fill; any prior contents are
 *                           overwritten.
 * \param[in]  bbox          Bounding box.
 * \param[in]  fg            Groove/fill colour.
 * \param[in]  orientation   Groove direction.
 * \param[in]  min           Value at the groove's start.
 * \param[in]  max           Value at the groove's end.
 * \param[in]  default_value Initial value, clamped to [min,max].
 */
void wuss_icon_spec_slider(wuss_icon_spec_t         *spec,
                           box_t                     bbox,
                           wuss_colour_t             fg,
                           wuss_slider_orientation_t orientation,
                           int                       min,
                           int                       max,
                           int                       default_value);

/**
 * Fill \p spec as a wuss_ICON_TYPE_ACTION button.
 *
 * \param[out] spec    Spec to fill; any prior contents are overwritten.
 * \param[in]  bbox    Bounding box.
 * \param[in]  text    Button text, borrowed until wuss_icon_create(_array)
 *                     copies it.
 * \param[in]  fg      Text colour.
 * \param[in]  bg      Fill colour.
 * \param[in]  is_default Non-zero to draw as the dialogue's default action
 *                        (wuss_ICON_FLAGS_DEFAULT) instead of an ordinary
 *                        button.
 */
void wuss_icon_spec_action(wuss_icon_spec_t *spec,
                           box_t             bbox,
                           const char       *text,
                           wuss_colour_t     fg,
                           wuss_colour_t     bg,
                           int               is_default);

#endif /* WUSS_ICONS */

#ifdef __cplusplus
}
#endif

#endif /* WUSS_ICON_SPEC_H */
