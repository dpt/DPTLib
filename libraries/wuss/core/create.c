/* wuss/create.c -- wuss - minimal window manager */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"

#include "impl.h"

/* The default allocator: plain stdlib. wuss_create copies this in when its
 * alloc argument is NULL. */
const wuss_alloc_t wuss_alloc =
{
  malloc, realloc, free
};

result_t wuss_create(screen_t               *scr,
                     const wuss_font_desc_t *fonts,
                     int                     nfonts,
                     const colour_t         *palette,
                     int                     npalette,
                     const wuss_config_t    *config,
                     const wuss_alloc_t     *alloc,
                     const char             *resources,
                     wuss_t                **wuss)
{
  /* Built-in fallback palette when the caller passes NULL: black and white.
   * wuss makes no other assumptions about palette contents or length. */
  static const colour_t default_palette[] =
  {
    { 0xFF000000 }, /* 0: black */
    { 0xFFFFFFFF }  /* 1: white */
  };

  result_t     rc;
  wuss_alloc_t al;
  wuss_t      *w;
  int          s;

  assert(scr  != NULL);
  assert(wuss != NULL);

  if (nfonts < 0 || nfonts > wuss_MAX_FONTS || (nfonts > 0 && fonts == NULL))
    return result_BAD_ARG;

  al = (alloc != NULL) ? *alloc : wuss_alloc;

  w = al.malloc(sizeof(*w));
  if (w == NULL)
    return result_OOM;
  w->alloc = al;

  wuss__fontset_init(&w->fonts, fonts, nfonts);

  if (palette == NULL)
  {
    palette  = default_palette;
    npalette = NELEMS(default_palette);
  }
  else if (npalette <= 0)
  {
    wuss__free(w, w);
    return result_BAD_ARG;
  }

  w->palette = wuss__malloc(w, npalette * sizeof(*w->palette));
  if (w->palette == NULL)
  {
    wuss__free(w, w);
    return result_OOM;
  }
  memcpy(w->palette, palette, npalette * sizeof(*w->palette));
  w->npalette = npalette;

  rc = wuss__apply_config(w, config);
  if (rc != result_OK)
  {
    wuss__free(w, w->palette);
    wuss__free(w, w);
    return rc;
  }

  w->scr                = scr;
  w->resources          = resources;
  w->pointer_window     = NULL;
  w->focus              = NULL;
#ifdef WUSS_FURNITURE
  w->furniture.dragging       = NULL;
  w->furniture.drag.x         = 0;
  w->furniture.drag.y         = 0;
  w->furniture.pressed_region = wuss_FURNITURE_NONE;
  w->furniture_ops            = &wuss__furniture_default_ops;
#endif
#ifdef WUSS_ICONS
  w->pressed_icon       = NULL;
  w->pressed_window     = NULL;
  w->hover_icon         = NULL;
  w->hover_window       = NULL;
  w->caret_icon         = NULL;
  w->caret_window       = NULL;
  w->caret_index        = 0;
  w->icon.names         = NULL;
  w->icon.atoms         = NULL;
  w->icon.bitmaps       = NULL;
  w->icon.nbitmaps      = 0;
#endif
#ifdef WUSS_MENUS
  w->menu_chain         = NULL;
  w->menu_task          = NULL;
  w->menu_eat_up        = 0;
#endif
#ifdef WUSS_ICONBAR
  w->iconbar_window         = NULL;
  w->iconbar_task           = NULL;
  w->iconbar_icons          = NULL;
  w->niconbar_icons         = 0;
  w->cap_iconbar_icons      = 0;
  w->pressed_iconbar_icon   = NULL;
#endif

  w->queue          = NULL;
  w->nqueued        = 0;
  w->cap_queue      = 0;
  w->next_ref       = 1;
  w->dispatch_depth = 0;
  w->acking         = NULL;
  w->acked          = 0;

  w->ndirty   = 0;
  w->ntouched = 0;

  w->pointer.x = 0;
  w->pointer.y = 0;

  for (s = 0; s < WUSS_STACK_COUNT; s++)
    list_init(&w->z_order[s]);
  list_init(&w->tasks);

#ifdef WUSS_ICONS
  /* load the wuss-wide icon set now that w->resources is set; a missing
   * resources root or icons directory just leaves the set empty, not an
   * error -- callers needing the icon set check wuss_icons_count/_lookup */
  if (resources != NULL)
    (void) wuss_icons_load_resource(w, resources);
#endif

  if (bmfontcache_create(&w->font_cache) != result_OK)
  {
#ifdef WUSS_ICONS
    wuss__icons_registry_free(w);
#endif
    wuss__free(w, w->palette);
    wuss__free(w, w);
    return result_OOM;
  }

  *wuss = w;

  return result_OK;
}
