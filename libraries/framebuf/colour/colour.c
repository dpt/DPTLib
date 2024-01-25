/* colour.c */

#include <assert.h>
#include <stddef.h>
#include <limits.h>

#include "framebuf/pixelfmt.h"
#include "framebuf/colour.h"

colour_t colour_rgb(int r, int g, int b)
{
  return colour_rgba(r, g, b, PIXELFMT_OPAQUE);
}

colour_t colour_rgba(int r, int g, int b, int a)
{
  colour_t c;

  c.primary = PIXELFMT_MAKE_RGBA8888(r, g, b, a);
  return c;
}

static unsigned int closest_palette_entry(const colour_t *palette,
                                          int             nentries,
                                          colour_t        required)
{
#if 1
  /* libjpeg's weights */
  const int red_weight   = 19595; /* 0.29900 * 65536 (rounded down) */
  const int green_weight = 38470; /* 0.58700 * 65536 (rounded up)   */
  const int blue_weight  =  7471; /* 0.11400 * 65536 (rounded down) */
#else
  /* libpng's weights */
  const int red_weight   = 13938; /* 0.212671 * 65536 (rounded up)   */
  const int green_weight = 46869; /* 0.715160 * 65536 (rounded up)   */
  const int blue_weight  =  4729; /* 0.072169 * 65536 (rounded down) */
#endif

  pixelfmt_rgba8888_t req;
  int                 closest;
  unsigned int        dist;
  int                 req_r, req_g, req_b;
  int                 i;
  pixelfmt_rgba8888_t ent;
  int                 ent_r, ent_g, ent_b;
  int                 dr, dg, db;
  unsigned int        curdist;

  req = required.primary;
  req_r = PIXELFMT_Rxxx8888(req);
  req_g = PIXELFMT_xGxx8888(req);
  req_b = PIXELFMT_xxBx8888(req);

  closest = -1;
  dist    = INT_MAX;

  for (i = 0; i < nentries; i++)
  {
    ent = palette[i].primary;
    ent_r = PIXELFMT_Rxxx8888(ent);
    ent_g = PIXELFMT_xGxx8888(ent);
    ent_b = PIXELFMT_xxBx8888(ent);
    dr = ent_r - req_r;
    dg = ent_g - req_g;
    db = ent_b - req_b;
    curdist = ((dr * dr) * red_weight   +
               (dg * dg) * green_weight +
               (db * db) * blue_weight) >> 16;
    if (curdist < dist)
    {
      dist    = curdist;
      closest = i;
    }
  }

  return closest;
}

pixelfmt_any_t colour_to_pixel(const colour_t *palette,
                               int             nentries,
                               colour_t        required,
                               pixelfmt_t      fmt)
{
  pixelfmt_any_t r,g,b,a;

  switch (fmt)
  {
  case pixelfmt_p1:
  case pixelfmt_p2:
  case pixelfmt_p4:
  case pixelfmt_p8:
    return closest_palette_entry(palette, nentries, required);

  case pixelfmt_rgbx8888:
    return (PIXELFMT_OPAQUE << PIXELFMT_xxxA8888_SHIFT) | required.primary; /* ensure opaque */

  case pixelfmt_rgba8888:
    return required.primary;

  case pixelfmt_bgrx8888:
  case pixelfmt_bgra8888:
    r = PIXELFMT_Rxxx8888(required.primary);
    g = PIXELFMT_xGxx8888(required.primary);
    b = PIXELFMT_xxBx8888(required.primary);
    a = (fmt == pixelfmt_bgra8888) ? PIXELFMT_xxxA8888(required.primary) : PIXELFMT_OPAQUE;
    return (a << 24) | (r << 16) | (g << 8) | (b << 0); // need pixelfmt.h defs

  default:
    assert("Unimplemented pixel format" == NULL);
    return 0xFFFFFFFF;
  }
}

unsigned int colour_get_alpha(const colour_t *c)
{
  return PIXELFMT_xxxA8888(c->primary);
}
