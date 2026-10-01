/* wuss/set-config.c -- apply a wuss_config_t at create or mid-session */

#include <assert.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"

#include "impl.h"

#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
/* Range-check the bevel colours and the backdrop against the palette. Shared
 * by the furniture and icons-only paths so the accepted range and the error
 * code stay in one place. */
static result_t validate_bevel_backdrop(const wuss_t *w,
                                        wuss_colour_t blight,
                                        wuss_colour_t bdark,
                                        wuss_colour_t bdivider,
                                        wuss_colour_t btnbg,
                                        wuss_colour_t btnfg,
                                        wuss_colour_t btnpressed,
                                        wuss_colour_t accent)
{
  if (blight      >= w->npalette ||
      bdark       >= w->npalette ||
      bdivider    >= w->npalette ||
      btnbg       >= w->npalette ||
      btnfg       >= w->npalette ||
      btnpressed  >= w->npalette ||
      accent      >= w->npalette ||
      wuss__validate_backdrop(w, &w->backdrop) != result_OK)
    return result_WUSS_BAD_COLOUR;

  return result_OK;
}
#endif

result_t wuss__apply_config(wuss_t *w, const wuss_config_t *config)
{
#ifdef WUSS_FURNITURE
  wuss_furniture_palette_t pal;
  wuss_colour_t            bg, fg;
#endif
#if defined(WUSS_FURNITURE) || defined(WUSS_ICONS)
  wuss_colour_t blight, bdark, bdivider;
  wuss_colour_t btnbg, btnfg, btnpressed, accent;
#endif
#ifdef WUSS_FURNITURE
  int            font_height;
#endif

  /* First pass: white/black and the named symbolic colours (BLACK..GREY).
   * The chrome-role slots are still the out-of-range sentinel here -- filled
   * once the config's own colours below are resolved and stored. This lets
   * wuss__resolve_colour handle a config that uses a named symbolic. */
  wuss__rebuild_palettecache(w);

  if (config != NULL)
  {
    w->backdrop            = config->backdrop;
    w->backdrop.colour     = wuss__resolve_colour(w, w->backdrop.colour);
    w->backdrop.pattern_bg = wuss__resolve_colour(w, w->backdrop.pattern_bg);
    w->window_bg           = wuss__resolve_colour(w, config->body.window);
    w->menu_bg             = wuss__resolve_colour(w, config->body.menu);
  }
  else
  {
    w->backdrop.colour     = wuss_NO_BACKGROUND;
    w->backdrop.pattern    = screen_PATTERN_SOLID;
    w->backdrop.pattern_bg = wuss_NO_BACKGROUND;
    w->window_bg           = wuss__resolve_colour(w, wuss_COLOUR_GREY);
    w->menu_bg             = wuss__resolve_colour(w, wuss_COLOUR_WHITE);
  }

  if (w->window_bg >= w->npalette || w->menu_bg >= w->npalette)
    return result_WUSS_BAD_COLOUR;

#ifdef WUSS_FURNITURE
  if (config != NULL)
  {
    pal = config->furniture;
    pal.title.bg        = wuss__resolve_colour(w, pal.title.bg);
    pal.title.fg        = wuss__resolve_colour(w, pal.title.fg);
    pal.title.focus_bg  = (pal.title.focus_bg == wuss_NO_BACKGROUND)
                        ? pal.title.bg
                        : wuss__resolve_colour(w, pal.title.focus_bg);
    pal.outline         = wuss__resolve_colour(w, pal.outline); /* NO_BACKGROUND: follows the titlebar fill, see furniture/draw.c */
    pal.back            = wuss__resolve_colour(w, pal.back);
    pal.close           = wuss__resolve_colour(w, pal.close);
    pal.toggle          = wuss__resolve_colour(w, pal.toggle);
    pal.resize          = wuss__resolve_colour(w, pal.resize);
    pal.scroll.arrows   = wuss__resolve_colour(w, pal.scroll.arrows);
    pal.scroll.wells    = wuss__resolve_colour(w, pal.scroll.wells);
    pal.scroll.sausages = wuss__resolve_colour(w, pal.scroll.sausages);
    pal.pressed         = wuss__resolve_colour(w, pal.pressed);
    blight     = wuss__resolve_colour(w, config->bevel.light);
    bdark      = wuss__resolve_colour(w, config->bevel.dark);
    bdivider   = (config->bevel.divider == wuss_NO_BACKGROUND)
               ? blight
               : wuss__resolve_colour(w, config->bevel.divider);
    btnbg      = (config->button.bg == wuss_NO_BACKGROUND)
               ? blight
               : wuss__resolve_colour(w, config->button.bg);
    btnfg      = wuss__resolve_colour(w, config->button.fg);
    btnpressed = (config->button.pressed == wuss_NO_BACKGROUND)
               ? bdark
               : wuss__resolve_colour(w, config->button.pressed);
    accent     = wuss__resolve_colour(w, config->accent.colour);
  }
  else
  {
    bg = 0;
    fg = (w->npalette > 1) ? 1 : 0;

    pal.title.bg        = bg;
    pal.title.fg        = fg;
    pal.title.focus_bg  = wuss_nearest_colour(w, 0xFF, 0xEE, 0xAA); /* cream */
    pal.outline         = bg;
    pal.back            = fg;
    pal.close           = fg;
    pal.toggle          = fg;
    pal.resize          = bg;
    pal.scroll.arrows   = bg;
    pal.scroll.wells    = bg;
    pal.scroll.sausages = fg;
    pal.pressed         = fg;

    blight     = 0;
    bdark      = 0;
    bdivider   = 0;
    btnbg      = 0;
    btnfg      = fg;
    btnpressed = 0;
    accent     = bg; /* default action button: the titlebar fill */
  }

  if (pal.title.bg        >= w->npalette ||
      pal.title.fg        >= w->npalette ||
      pal.title.focus_bg  >= w->npalette ||
      (pal.outline        >= w->npalette &&
       pal.outline        != wuss_NO_BACKGROUND) ||
      pal.back            >= w->npalette ||
      pal.close           >= w->npalette ||
      pal.toggle          >= w->npalette ||
      pal.resize          >= w->npalette ||
      pal.scroll.arrows   >= w->npalette ||
      pal.scroll.wells    >= w->npalette ||
      pal.scroll.sausages >= w->npalette ||
      pal.pressed         >= w->npalette ||
      validate_bevel_backdrop(w, blight, bdark, bdivider,
                              btnbg, btnfg, btnpressed, accent) != result_OK)
    return result_WUSS_BAD_COLOUR;

  w->furniture_colours = pal;
  w->bevel_light       = blight;
  w->bevel_dark        = bdark;
  w->bevel_divider     = bdivider;
  w->button_bg         = btnbg;
  w->button_fg         = btnfg;
  w->button_pressed    = btnpressed;
  w->accent            = accent;

  if (config != NULL && config->titlebar_height > 0)
  {
    w->titlebar_height = config->titlebar_height;
  }
  else
  {
    /* titles draw in the bold weight when one was supplied; size the
     * titlebar to whichever weight is taller. Both slots empty -> fall back
     * to the default. */
    font_height = MAX(wuss__fontset_height(&w->fonts, 0),
                      wuss__fontset_height(&w->fonts, 1));
    w->titlebar_height = (font_height > 0) ? font_height + 4
                                           : WUSS_DEFAULT_TITLEBAR_HEIGHT;
  }
#else /* !WUSS_FURNITURE */
#ifdef WUSS_ICONS
  if (config != NULL)
  {
    blight     = wuss__resolve_colour(w, config->bevel.light);
    bdark      = wuss__resolve_colour(w, config->bevel.dark);
    bdivider   = (config->bevel.divider == wuss_NO_BACKGROUND)
               ? blight
               : wuss__resolve_colour(w, config->bevel.divider);
    btnbg      = (config->button.bg == wuss_NO_BACKGROUND)
               ? blight
               : wuss__resolve_colour(w, config->button.bg);
    btnfg      = wuss__resolve_colour(w, config->button.fg);
    btnpressed = (config->button.pressed == wuss_NO_BACKGROUND)
               ? bdark
               : wuss__resolve_colour(w, config->button.pressed);
    accent     = wuss__resolve_colour(w, config->accent.colour);
  }
  else
  {
    blight     = 0;
    bdark      = 0;
    bdivider   = 0;
    btnbg      = 0;
    btnfg      = (w->npalette > 1) ? 1 : 0;
    btnpressed = 0;
    accent     = 0;
  }
  if (validate_bevel_backdrop(w, blight, bdark, bdivider,
                              btnbg, btnfg, btnpressed, accent) != result_OK)
    return result_WUSS_BAD_COLOUR;
  w->bevel_light    = blight;
  w->bevel_dark     = bdark;
  w->bevel_divider  = bdivider;
  w->button_bg      = btnbg;
  w->button_fg      = btnfg;
  w->button_pressed = btnpressed;
  w->accent         = accent;
#else
  if (wuss__validate_backdrop(w, &w->backdrop) != result_OK)
    return result_WUSS_BAD_COLOUR;
#endif
#endif /* WUSS_FURNITURE */

  if (config != NULL && config->double_click_ms > 0)
    w->double_click_ms = config->double_click_ms;
  else
    w->double_click_ms = WUSS_DEFAULT_DOUBLE_CLICK_MS;

  if (config != NULL && config->double_click_px > 0)
    w->double_click_px = config->double_click_px;
  else
    w->double_click_px = WUSS_DEFAULT_DOUBLE_CLICK_PX;

  if (config != NULL && config->drag_threshold_px > 0)
    w->drag_threshold_px = config->drag_threshold_px;
  else
    w->drag_threshold_px = WUSS_DEFAULT_DRAG_THRESHOLD_PX;

#ifdef WUSS_ICONS
  {
    wuss_colour_t track, value, surround;

    if (config != NULL)
    {
      track = (config->slider.track == wuss_NO_BACKGROUND)
            ? w->window_bg
            : wuss__resolve_colour(w, config->slider.track);
      value = (config->slider.value == wuss_NO_BACKGROUND)
            ? w->button_bg
            : wuss__resolve_colour(w, config->slider.value);
      surround = (config->slider.surround == wuss_NO_BACKGROUND)
               ? w->button_bg
               : wuss__resolve_colour(w, config->slider.surround);
    }
    else
    {
      track    = w->window_bg;
      value    = w->button_bg;
      surround = w->button_bg;
    }

    if (track >= w->npalette || value >= w->npalette || surround >= w->npalette)
      return result_WUSS_BAD_COLOUR;

    w->slider_track    = track;
    w->slider_value    = value;
    w->slider_surround = surround;
  }
#endif

  /* Second pass: the chrome colours are stored and concrete now, so fill in
   * the chrome-role symbolic slots (wuss_COLOUR_TITLE_BG etc.). */
  wuss__rebuild_palettecache(w);

  return result_OK;
}

result_t wuss_set_config(wuss_t *wuss, const wuss_config_t *config)
{
  result_t     rc;
  wuss_t      *saved;
  wuss_event_t event;
  list_t      *e;
  box_t        screen;

  assert(wuss   != NULL);
  assert(config != NULL);

  /* wuss__apply_config stores as it goes, so snapshot the lot to roll back
   * a config that fails validation part way through */
  saved = wuss__malloc(wuss, sizeof(*saved));
  if (saved == NULL)
    return result_OOM;

  *saved = *wuss;

  rc = wuss__apply_config(wuss, config);
#ifdef WUSS_FURNITURE
  /* ponytail: titlebar height is creation-only -- changing it would move
   * every window's visible box; recompute those if it's ever needed */
  wuss->titlebar_height = saved->titlebar_height;
#endif
  if (rc != result_OK)
    *wuss = *saved;
  wuss__free(wuss, saved);
  if (rc != result_OK)
    return rc;

  event.kind = wuss_EVENT_PALETTE;
  for (e = wuss->tasks.next; e != NULL; )
  {
    result_t     crc;
    wuss_task_t *task;
    list_t      *next;

    task = (wuss_task_t *) e;
    next = e->next; /* an autoclose task may free its own node in the handler */

    crc = wuss__deliver(task, NULL, &event);
    if (crc != result_OK && rc == result_OK)
      rc = crc;

    e = next;
  }

  screen.x0 = 0;
  screen.y0 = 0;
  screen.x1 = wuss->scr->size.w;
  screen.y1 = wuss->scr->size.h;
  wuss_invalidate(wuss, &screen);

  return rc;
}
