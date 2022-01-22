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
  int i;
  int closest = -1;
  int dist    = INT_MAX;

  for (i = 0; i < nentries; i++)
  {
    pixelfmt_rgba8888_t ent = palette[i].primary;
    pixelfmt_rgba8888_t req = required.primary;
    int ent_r = PIXELFMT_Rxxx8888(ent);
    int ent_g = PIXELFMT_xGxx8888(ent);
    int ent_b = PIXELFMT_xxBx8888(ent);
    int req_r = PIXELFMT_Rxxx8888(req);
    int req_g = PIXELFMT_xGxx8888(req);
    int req_b = PIXELFMT_xxBx8888(req);
    int dr = ent_r - req_r;
    int dg = ent_g - req_g;
    int db = ent_b - req_b;
    int newdist = (dr * dr) + (dg * dg) + (db * db); // Note: no weights yet
    if (newdist < dist)
    {
      dist    = newdist;
      closest = i;
    }
  }

  return closest;
}

pixelfmt_x_t colour_to_pixel(const colour_t *palette,
                             int             nentries,
                             colour_t        required,
                             pixelfmt_t      fmt)
{
  pixelfmt_x_t r,g,b,a;

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
