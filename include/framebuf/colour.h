/* colour.h -- colour type */

#ifndef FRAMEBUF_COLOUR_H
#define FRAMEBUF_COLOUR_H

#include "framebuf/pixelfmt.h"

typedef struct colour colour_t;

struct colour
{
  pixelfmt_rgba8888_t primary;
};

/** Create a colour from (R,G,B). */
colour_t colour_rgb(int r, int g, int b);

/** Create a colour from (R,G,B,A). */
colour_t colour_rgba(int r, int g, int b, int a);

/** Return colour `c` as a pixel of format `fmt`. */
pixelfmt_x_t colour_to_pixel(const colour_t *palette,
                             int             nentries,
                             colour_t        required,
                             pixelfmt_t      fmt);

/** Return the alpha component of the specified colour. */
unsigned int colour_get_alpha(const colour_t *c);

#endif /* FRAMEBUF_COLOUR_H */
