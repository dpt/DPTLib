/* framebuf/span/p2.c -- P2 (2bpp paletted) format plot methods */

#include <stddef.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-p2.h"

#define SPAN_P2_NENTRIES 4
#define SPAN_P2_MASK     3

/* pixel values here are unpacked palette indices, one per byte (not the
 * four-pixels-per-byte layout used in screen memory); packing/unpacking
 * into the actual screen bytes is the caller's job, as with
 * screen_set_pixel's other pixel formats. src2 is an array of colour_t,
 * not pre-quantised pixels, so the blend happens in full RGB precision
 * before re-quantising to the nearest palette entry. */
static void span_p2_blendconst(void       *vdst,
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

    old_colour = palette[*psrc1 & SPAN_P2_MASK];
    oldpx      = old_colour.primary;
    old_r      = PIXELFMT_Rxxx8888(oldpx);
    old_g      = PIXELFMT_xGxx8888(oldpx);
    old_b      = PIXELFMT_xxBx8888(oldpx);

    new_colour = *psrc2;
    newpx      = new_colour.primary;
    new_r      = PIXELFMT_Rxxx8888(newpx);
    new_g      = PIXELFMT_xGxx8888(newpx);
    new_b      = PIXELFMT_xxBx8888(newpx);

    blend_r = (new_r * alpha + old_r * (255 - alpha)) / 255;
    blend_g = (new_g * alpha + old_g * (255 - alpha)) / 255;
    blend_b = (new_b * alpha + old_b * (255 - alpha)) / 255;

    blended = colour_rgb(blend_r, blend_g, blend_b);
    *pdst   = (unsigned char) colour_to_pixel(palette, SPAN_P2_NENTRIES,
                                              blended, pixelfmt_p2);

    pdst++;
    psrc1++;
    psrc2++;
  }
}

/* unlike span_p2_blendconst above, this works directly on packed screen
 * bytes: "dst" is the row base, "first" the pixel index into it, so odd
 * start columns and odd lengths are handled without the caller packing.
 * "pixel" is a palette index in its low two bits. Bits 7..6 of a byte are
 * the leftmost pixel, matching the MSB-first sub-byte packing used
 * elsewhere in framebuf. */
static void span_p2_fill(void          *vdst,
                         int            first,
                         pixelfmt_any_t pixel,
                         int            length)
{
  unsigned char *base;
  unsigned char  idx;
  int            x;

  base = vdst;
  idx  = (unsigned char) (pixel & SPAN_P2_MASK);

  for (x = first; x < first + length; x++)
  {
    unsigned char *p;
    int            shift;

    p     = base + (x >> 2);
    shift = 6 - ((x & 3) << 1);
    *p    = (unsigned char) ((*p & ~(SPAN_P2_MASK << shift)) | (idx << shift));
  }
}

const span_t span_p2 =
{
  pixelfmt_p2,
  NULL, /* copy: unneeded so far (bit packing makes a generic array copy awkward) */
  span_p2_fill,
  span_p2_blendconst,
  NULL, /* blendarray: unneeded so far */
};
