/* wuss/helpers/icon-spec.c -- fill helpers for common wuss_icon_spec_t shapes */

#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "wuss/icon-spec.h"

#ifdef WUSS_ICONS

/* ----------------------------------------------------------------------- */

void wuss_icon_spec_label(wuss_icon_spec_t *spec,
                          box_t             bbox,
                          const char       *text,
                          wuss_colour_t     fg,
                          int               justify_right)
{
  memset(spec, 0, sizeof(*spec));
  spec->bbox  = bbox;
  spec->type  = wuss_ICON_TYPE_LABEL;
  spec->text  = text;
  spec->fg    = fg;
  spec->bg    = wuss_NO_BACKGROUND;
  spec->flags = justify_right ? wuss_ICON_FLAGS_JUSTIFY_RIGHT
                              : wuss_ICON_FLAGS_NONE;
}

void wuss_icon_spec_slider(wuss_icon_spec_t         *spec,
                           box_t                     bbox,
                           wuss_colour_t             fg,
                           wuss_slider_orientation_t orientation,
                           int                       min,
                           int                       max,
                           int                       default_value)
{
  memset(spec, 0, sizeof(*spec));
  spec->bbox = bbox;
  spec->type = wuss_ICON_TYPE_SLIDER;
  spec->fg   = fg;
  spec->bg   = wuss_NO_BACKGROUND;
  spec->u.slider.orientation   = orientation;
  spec->u.slider.min           = min;
  spec->u.slider.max           = max;
  spec->u.slider.default_value = default_value;
}

void wuss_icon_spec_action(wuss_icon_spec_t *spec,
                           box_t             bbox,
                           const char       *text,
                           wuss_colour_t     fg,
                           wuss_colour_t     bg,
                           int               is_default)
{
  memset(spec, 0, sizeof(*spec));
  spec->bbox  = bbox;
  spec->type  = wuss_ICON_TYPE_ACTION;
  spec->text  = text;
  spec->fg    = fg;
  spec->bg    = bg;
  spec->flags = is_default ? wuss_ICON_FLAGS_DEFAULT : wuss_ICON_FLAGS_NONE;
}

#endif /* WUSS_ICONS */
