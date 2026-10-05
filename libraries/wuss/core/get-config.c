/* wuss/get-config.c -- wuss - minimal window manager */

#include <assert.h>
#include <string.h>

#include "impl.h"

void wuss_get_config(const wuss_t *wuss, wuss_config_t *config)
{
  assert(wuss   != NULL);
  assert(config != NULL);

  memset(config, 0, sizeof(*config));

#ifdef WUSS_FURNITURE
  config->titlebar_height = wuss->titlebar_height;
  config->furniture       = wuss->furniture_colours;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  config->bevel.light      = wuss->bevel.light;
  config->bevel.dark       = wuss->bevel.dark;
  config->bevel.divider    = wuss->bevel.divider;
  config->button.bg        = wuss->button.bg;
  config->button.fg        = wuss->button.fg;
  config->button.pressed   = wuss->button.pressed;
  config->accent.colour    = wuss->accent;
#endif
#ifdef WUSS_ICONS
  config->slider.track     = wuss->slider.track;
  config->slider.value     = wuss->slider.value;
  config->slider.surround  = wuss->slider.surround;
#endif

  config->body.window = wuss->body.window;
  config->body.menu   = wuss->body.menu;
  config->backdrop    = wuss->backdrop;

  config->mouse.double_click_ms   = wuss->input.double_click_ms;
  config->mouse.double_click_px   = wuss->input.double_click_px;
  config->mouse.drag_threshold_px = wuss->input.drag_threshold_px;
}
