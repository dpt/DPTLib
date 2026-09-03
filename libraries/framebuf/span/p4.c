/* framebuf/span/p4.c -- P4 (4bpp paletted) format plot methods */

#include <stddef.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-p4.h"

#define SPAN_P4_NENTRIES 16

/* pixel values here are unpacked palette indices, one per byte (not the
 * two-nibbles-per-byte layout used in screen memory); packing/unpacking
 * into the actual screen bytes is the caller's job, as with
 * screen_set_pixel's other pixel formats. src2 is an array of colour_t,
 * not pre-quantised pixels, so the blend happens in full RGB precision
 * before re-quantising to the nearest palette entry. */
static void span_p4_blendconst(void       *vdst,
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
    *pdst   = (unsigned char) colour_to_pixel(palette, SPAN_P4_NENTRIES, blended, pixelfmt_p4);

    pdst++;
    psrc1++;
    psrc2++;
  }
}

/* unlike span_p4_blendconst above, this works directly on packed screen
 * bytes: "dst" is the row base, "first" the nibble (pixel) index into it, so
 * odd start columns and odd lengths are handled without the caller packing.
 * "pixel" is a palette index in its low nibble. */
static void span_p4_fill(void          *vdst,
                         int            first,
                         pixelfmt_any_t pixel,
                         int            length)
{
  unsigned char *base;
  unsigned char  nib;
  int            x;

  base = vdst;
  nib  = (unsigned char) (pixel & 0xF);

  for (x = first; x < first + length; x++)
  {
    unsigned char *p;
    int            shift;

    p     = base + (x >> 1);
    shift = (x & 1) * 4;
    *p    = (unsigned char) ((*p & ~(0xF << shift)) | (nib << shift));
  }
}

const span_t span_p4 =
{
  pixelfmt_p4,
  NULL, /* copy: unneeded so far (nibble packing makes a generic array copy awkward) */
  span_p4_fill,
  span_p4_blendconst,
  NULL, /* blendarray: unneeded so far */
};
