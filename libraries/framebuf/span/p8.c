/* framebuf/span/p8.c -- P8 (8bpp paletted) format plot methods */

#include <stddef.h>
#include <string.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-p8.h"

#define SPAN_P8_NENTRIES 256

/* whole-byte format: one palette index per byte, so a run of pixels is a
 * plain contiguous byte range and copy is a straight memcpy. */
static void span_p8_copy(void *vdst, const void *vsrc, int length)
{
  memcpy(vdst, vsrc, (size_t) length);
}

/* like span_p4_blendconst, but p8 is a plain byte store so there is no
 * nibble packing: "src1" is one index per byte, "src2" an array of colour_t
 * blended in full RGB precision before re-quantising to the nearest palette
 * entry, "context" the palette. */
static void span_p8_blendconst(void       *vdst,
                               const void *vsrc1,
                               const void *vsrc2,
                               int         length,
                               int         alpha,
                               const void *context)
{
  unsigned char       *pdst;
  const unsigned char *psrc1;
  const colour_t      *psrc2;
  const colour_t      *palette;

  pdst    = vdst;
  psrc1   = vsrc1;
  psrc2   = vsrc2;
  palette = context;

  while (length--)
  {
    colour_t            old_colour, new_colour, blended;
    pixelfmt_rgba8888_t oldpx, newpx;
    int                 old_r, old_g, old_b;
    int                 new_r, new_g, new_b;
    int                 blend_r, blend_g, blend_b;

    old_colour = palette[*psrc1];
    oldpx = old_colour.primary;
    old_r = PIXELFMT_Rxxx8888(oldpx);
    old_g = PIXELFMT_xGxx8888(oldpx);
    old_b = PIXELFMT_xxBx8888(oldpx);

    new_colour = *psrc2;
    newpx = new_colour.primary;
    new_r = PIXELFMT_Rxxx8888(newpx);
    new_g = PIXELFMT_xGxx8888(newpx);
    new_b = PIXELFMT_xxBx8888(newpx);

    blend_r = (new_r * alpha + old_r * (255 - alpha)) / 255;
    blend_g = (new_g * alpha + old_g * (255 - alpha)) / 255;
    blend_b = (new_b * alpha + old_b * (255 - alpha)) / 255;

    blended = colour_rgb(blend_r, blend_g, blend_b);
    *pdst   = (unsigned char) colour_to_pixel(palette, SPAN_P8_NENTRIES,
                                              blended, pixelfmt_p8);

    pdst++;
    psrc1++;
    psrc2++;
  }
}

/* "dst" is the row base, "first" the pixel index into it; whole-byte, so
 * this is just a byte store. "pixel" is a palette index in its low byte. */
static void span_p8_fill(void          *vdst,
                         int            first,
                         pixelfmt_any_t pixel,
                         int            length)
{
  unsigned char *base;

  base = vdst;
  memset(base + first, (unsigned char) (pixel & 0xFF), (size_t) length);
}

const span_t span_p8 =
{
  pixelfmt_p8,
  span_p8_copy,
  span_p8_fill,
  span_p8_blendconst,
  NULL, /* blendarray: unneeded so far */
};
