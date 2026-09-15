/* wuss/helpers/icon-spec.c -- fill helpers for common wuss_icon_spec_t shapes */

#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "wuss/icon-spec.h"

#ifdef WUSS_ICONS

/* value label buffer: ample for any int plus a short unit suffix in fmt */
#define SLIDER_ROW_BUF 32

/* nearest multiple of step above min, clamped to [min,max] */
static int wuss__slider_row_snap(int v, int min, int max, int step)
{
  v = ((v - min + step / 2) / step) * step + min;

  return CLAMP(v, min, max);
}

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

void wuss_icon_spec_slider_row(wuss_icon_spec_t         *slider_spec,
                               wuss_icon_spec_t         *value_spec,
                               box_t                     slider_bbox,
                               box_t                     value_bbox,
                               wuss_colour_t             fg,
                               wuss_slider_orientation_t orientation,
                               int                       min,
                               int                       max,
                               int                       default_value,
                               const char               *fmt,
                               int                       step)
{
  char buf[SLIDER_ROW_BUF];

  if (step != 0)
    default_value = wuss__slider_row_snap(default_value, min, max, step);
  wuss_icon_spec_slider(slider_spec, slider_bbox, fg, orientation, min, max,
                        default_value);
  snprintf(buf, sizeof(buf), fmt ? fmt : "%d", default_value);
  wuss_icon_spec_label(value_spec, value_bbox, buf, fg, 0);
}

void wuss_slider_row_bind(wuss_slider_row_t *row,
                          wuss_icon_t       *slider,
                          wuss_icon_t       *value,
                          const char        *fmt,
                          int                min,
                          int                max,
                          int                step)
{
  row->slider = slider;
  row->value  = value;
  row->fmt    = fmt;
  row->min    = min;
  row->max    = max;
  row->step   = step;
}

result_t wuss_slider_row_set(wuss_window_t           *window,
                             const wuss_slider_row_t *row,
                             int                      value)
{
  char buf[SLIDER_ROW_BUF];

  if (row->step != 0)
    value = wuss__slider_row_snap(value, row->min, row->max, row->step);
  wuss_icon_set_value(window, row->slider, value);
  snprintf(buf, sizeof(buf), row->fmt ? row->fmt : "%d",
           wuss_icon_get_value(row->slider));
  return wuss_icon_set_text(window, row->value, buf);
}

int wuss_slider_row_event(wuss_window_t           *window,
                          const wuss_slider_row_t *row,
                          const wuss_event_t      *event,
                          int                     *value)
{
  char buf[SLIDER_ROW_BUF];
  int  v;

  if (event->kind != wuss_EVENT_ICON || event->data.icon.icon != row->slider)
    return 0;
  if (event->data.icon.action != wuss_MOUSE_DOWN &&
      event->data.icon.action != wuss_MOUSE_MOVE)
    return 0;

  v = event->data.icon.value;
  if (row->step != 0)
  {
    v = wuss__slider_row_snap(v, row->min, row->max, row->step);
    wuss_icon_set_value(window, row->slider, v); /* visibly jump to it */
  }
  snprintf(buf, sizeof(buf), row->fmt ? row->fmt : "%d", v);
  wuss_icon_set_text(window, row->value, buf);

  if (value != NULL)
    *value = v;
  return 1;
}

#endif /* WUSS_ICONS */
