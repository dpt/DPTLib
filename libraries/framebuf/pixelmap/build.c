/* framebuf/pixelmap/build.c -- fill a pixelmap's entry table */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "impl.h"

/* paletted -> deep: one destination pixel per palette entry, index = the raw
 * palette index. */
static int pixelmap__build_paletted_to_deep(pixelmap_t     *pm,
                                            const colour_t *palette,
                                            int             nentries)
{
  uint32_t *entries;
  int       i;

  entries = calloc(pm->nentries, sizeof(*entries));
  if (entries == NULL)
    return -1;

  for (i = 0; i < nentries; i++)
    entries[i] = (uint32_t) colour_to_pixel(palette, nentries,
                                            palette[i], pm->destfmt);

  pm->entries = (const unsigned char *) entries;

  return 0;
}

/* Replicate an n-bit value up to 8 bits (e.g. 4-bit 0xA -> 0xAA), so a
 * quantised channel spans the full 0..255 range before the nearest-colour
 * match. */
static unsigned int pixelmap__expand(unsigned int v, unsigned int nbits)
{
  unsigned int out;
  unsigned int filled;

  out    = 0;
  filled = 0;

  while (filled < 8)
  {
    out     = (out << nbits) | v;
    filled += nbits;
  }

  return (out >> (filled - 8)) & 0xFF;
}

int pixelmap__build_table(pixelmap_t     *pm,
                          const colour_t *palette,
                          int             nentries)
{
  unsigned char *entries;
  size_t         nbytes;
  unsigned int   per_byte;
  unsigned int   rbits, gbits, bbits;
  unsigned int   r, g, b;
  unsigned int   idx;
  unsigned int   pal;
  colour_t       c;

  if (pm->entry_bytes != 0)
    return pixelmap__build_paletted_to_deep(pm, palette, nentries);

  rbits = pm->rbits;
  gbits = pm->gbits;
  bbits = pm->bbits;

  per_byte = 8u >> pm->dest_log2bpp; /* p1:8 p2:4 p4:2 p8:1 */
  nbytes   = (pm->nentries + per_byte - 1) / per_byte;

  entries = calloc(nbytes, 1);
  if (entries == NULL)
    return -1;

  for (r = 0; r < (1u << rbits); r++)
    for (g = 0; g < (1u << gbits); g++)
      for (b = 0; b < (1u << bbits); b++)
      {
        idx = (r << (gbits + bbits)) | (g << bbits) | b;

        c = colour_rgb((int) pixelmap__expand(r, rbits),
                       (int) pixelmap__expand(g, gbits),
                       (int) pixelmap__expand(b, bbits));

        pal = colour_to_pixel(palette, nentries, c, pm->destfmt)
            & ((1u << (1u << pm->dest_log2bpp)) - 1);

        /* pack LSB-first: p8 is a plain byte store */
        if (pm->dest_log2bpp == 3)
          entries[idx] = (unsigned char) pal;
        else
        {
          unsigned int bit;

          bit = (idx % per_byte) << pm->dest_log2bpp;
          entries[idx / per_byte] |= (unsigned char) (pal << bit);
        }
      }

  pm->entries = entries;

  return 0;
}
