/* wuss/get-config.c -- wuss - minimal window manager */

#include <assert.h>
#include <string.h>

#include "impl.h"

void wuss_get_config(const wuss_t *wuss, wuss_config_t *config)
{
  assert(wuss   != NULL);
  assert(config != NULL);

  memset(config, 0, sizeof(*config));

  config->titlebar_height = wuss->titlebar_height;

#ifdef WUSS_FURNITURE
  config->furniture = wuss->furniture_colours;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  config->bevel.light      = wuss->bevel_light;
  config->bevel.dark       = wuss->bevel_dark;
  config->bevel.divider    = wuss->bevel_divider;
  config->button.bg        = wuss->button_bg;
  config->button.fg        = wuss->button_fg;
  config->button.pressed   = wuss->button_pressed;
  config->accent.colour    = wuss->accent;
#endif
#ifdef WUSS_ICONS
  config->slider.track     = wuss->slider_track;
  config->slider.value     = wuss->slider_value;
  config->slider.surround  = wuss->slider_surround;
#endif

  config->body.window = wuss->window_bg;
  config->body.menu   = wuss->menu_bg;
  config->backdrop    = wuss->backdrop;

  config->double_click_ms   = wuss->double_click_ms;
  config->double_click_px   = wuss->double_click_px;
  config->drag_threshold_px = wuss->drag_threshold_px;
}
