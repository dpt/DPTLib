/* framebuf/span/p1.c -- P1 (1bpp paletted) format plot methods */

#include <stddef.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-p1.h"

#define SPAN_P1_NENTRIES 2

/* pixel values here are unpacked palette indices, one per byte (not the
 * eight-bits-per-byte layout used in screen memory); packing/unpacking
 * into the actual screen bytes is the caller's job, as with
 * screen_set_pixel's other pixel formats. src2 is an array of colour_t,
 * not pre-quantised pixels, so the blend happens in full RGB precision
 * before re-quantising to the nearest palette entry. */
static void span_p1_blendconst(void       *vdst,
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
    colour_t old_colour, new_colour, blended;
    pixelfmt_rgba8888_t oldpx, newpx;
    int old_r, old_g, old_b;
    int new_r, new_g, new_b;
    int blend_r, blend_g, blend_b;

    old_colour = palette[*psrc1 & 1];
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
    *pdst   = (unsigned char) colour_to_pixel(palette, SPAN_P1_NENTRIES, blended, pixelfmt_p1);

    pdst++;
    psrc1++;
    psrc2++;
  }
}

/* unlike span_p1_blendconst above, this works directly on packed screen
 * bytes: "dst" is the row base, "first" the bit (pixel) index into it, so
 * odd start columns and odd lengths are handled without the caller packing.
 * "pixel" is a palette index in its low bit. Bit 7 of a byte is the
 * leftmost pixel, matching PNG's MSB-first 1bpp packing. */
static void span_p1_fill(void          *vdst,
                         int            first,
                         pixelfmt_any_t pixel,
                         int            length)
{
  unsigned char *base;
  unsigned char  bit;
  int            x;

  base = vdst;
  bit  = (unsigned char) (pixel & 1);

  for (x = first; x < first + length; x++)
  {
    unsigned char *p;
    int            shift;

    p     = base + (x >> 3);
    shift = 7 - (x & 7);
    *p    = (unsigned char) ((*p & ~(1 << shift)) | (bit << shift));
  }
}

const span_t span_p1 =
{
  pixelfmt_p1,
  NULL, /* copy: unneeded so far (bit packing makes a generic array copy awkward) */
  span_p1_fill,
  span_p1_blendconst,
  NULL, /* blendarray: unneeded so far */
};
