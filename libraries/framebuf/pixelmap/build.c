/* framebuf/pixelmap/build.c -- fill a pixelmap's entry table */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/utils.h"

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "impl.h"

/* paletted -> deep: one destination pixel per palette entry, index = the raw
 * palette index. entry_bytes is 2 for a 16bpp destination, else 4. */
static int pixelmap__build_paletted_to_deep(pixelmap_t     *pm,
                                            const colour_t *palette,
                                            int             nentries)
{
  int i;

  if (pm->entry_bytes == 2)
  {
    uint16_t *entries;

    entries = calloc(pm->nentries, sizeof(*entries));
    if (entries == NULL)
      return -1;

    for (i = 0; i < nentries; i++)
      entries[i] = (uint16_t) colour_to_pixel(palette, nentries,
                                              palette[i], pm->destfmt);

    pm->entries = (const unsigned char *) entries;
  }
  else
  {
    uint32_t *entries;

    entries = calloc(pm->nentries, sizeof(*entries));
    if (entries == NULL)
      return -1;

    for (i = 0; i < nentries; i++)
      entries[i] = (uint32_t) colour_to_pixel(palette, nentries,
                                              palette[i], pm->destfmt);

    pm->entries = (const unsigned char *) entries;
  }

  return 0;
}

/* Midpoint of the 8-bit range a quantised n-bit bucket "v" covers. Used as
 * the bucket's representative colour for the nearest-palette search: a
 * bit-replicated value (e.g. 4-bit bucket 0xC -> 0xCC=204) would skew every
 * bucket towards its top edge instead of its ~199 centre, biasing every
 * match towards brighter palette entries. */
static unsigned int pixelmap__bucket_centre(unsigned int v,
                                            unsigned int nbits)
{
  unsigned int shift;
  unsigned int span;

  shift = 8 - nbits;
  span  = 1u << shift;

  return (v << shift) + span / 2;
}

/* Largest single-channel difference between two colours. */
static unsigned int pixelmap__chan_dist(const colour_t *a,
                                        const colour_t *b)
{
  unsigned int ar, ag, ab;
  unsigned int br, bg, bb;
  unsigned int dr, dg, db;

  colour_get_rgb(a, &ar, &ag, &ab);
  colour_get_rgb(b, &br, &bg, &bb);
  dr = (ar > br) ? ar - br : br - ar;
  dg = (ag > bg) ? ag - bg : bg - ag;
  db = (ab > bb) ? ab - bb : bb - ab;

  return MAX(dr, MAX(dg, db));
}

/* Median, over the palette's entries, of the channel distance to the nearest
 * other entry: the typical gap an ordered dither must span. A duplicate entry
 * (padding) is not a neighbour. Returns 255 if no two entries differ.
 *
 * ponytail: O(n^2) in the palette size, run once per cache miss; the table
 * fill below already does far more work. */
static unsigned int pixelmap__palette_step(const colour_t *palette,
                                           int             nentries)
{
  unsigned int hist[256];
  unsigned int n;
  int          i;
  int          j;
  unsigned int best;
  unsigned int d;
  unsigned int step;
  unsigned int seen;

  memset(hist, 0, sizeof(hist));
  n = 0;

  for (i = 0; i < nentries; i++)
  {
    best = 256;
    for (j = 0; j < nentries; j++)
    {
      d = pixelmap__chan_dist(&palette[i], &palette[j]);
      if (d != 0 && d < best)
        best = d;
    }
    if (best == 256)
      continue;

    hist[best]++;
    n++;
  }

  if (n == 0)
    return 255;

  seen = 0;
  for (step = 1; step < 255; step++)
  {
    seen += hist[step];
    if (seen * 2 >= n)
      break;
  }

  return step;
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
  unsigned int   step;
  unsigned int   bucket;

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

        c = colour_rgb((int) pixelmap__bucket_centre(r, rbits),
                       (int) pixelmap__bucket_centre(g, gbits),
                       (int) pixelmap__bucket_centre(b, bbits));

        pal = colour_to_pixel(palette, nentries, c, pm->destfmt)
            & ((1u << (1u << pm->dest_log2bpp)) - 1);

        /* pack LSB-first: p8 is a plain byte store */
        if (pm->dest_log2bpp == 3)
        {
          entries[idx] = (unsigned char) pal;
        }
        else
        {
          unsigned int bit;

          bit = (idx % per_byte) << pm->dest_log2bpp;
          entries[idx / per_byte] |= (unsigned char) (pal << bit);
        }
      }

  pm->entries = entries;

  /* a palette finer than the quantiser still bands at the bucket width */
  step     = pixelmap__palette_step(palette, nentries);
  bucket   = 255u / ((1u << MIN(rbits, MIN(gbits, bbits))) - 1);
  pm->step = (unsigned char) MAX(step, bucket);

  return 0;
}
