/* framebuf/bitmap/has-transparency.c -- whether any pixel is less than opaque */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/bitmap.h"

/* ----------------------------------------------------------------------- */

int bitmap_has_transparency(const bitmap_t *bm)
{
  int                     nentries;
  int                     i;
  int                     shift;
  int                     y;
  const pixelfmt_any32_t *row;
  int                     x;

  assert(bm);

  if (bitmap_is_compressed(bm))
    return 0;

  nentries = pixelfmt_paletted_nentries(bm->format);
  if (nentries > 0)
  {
    if (bm->palette == NULL)
      return 0;

    for (i = 0; i < nentries; i++)
      if (colour_get_alpha(&bm->palette[i]) != 0xFF)
        return 1;

    return 0;
  }

  if (!pixelfmt_has_alpha(bm->format))
    return 0;

  /* bgra and rgba keep alpha in the top byte; abgr and argb in the bottom */
  shift = (bm->format == pixelfmt_bgra8888 ||
           bm->format == pixelfmt_rgba8888) ? 24 : 0;

  for (y = 0; y < bm->size.h; y++)
  {
    row = (const pixelfmt_any32_t *) ((const uint8_t *) bm->base +
                                      (size_t) y * bm->rowbytes);
    for (x = 0; x < bm->size.w; x++)
      if (((row[x] >> shift) & 0xFF) != 0xFF)
        return 1;
  }

  return 0;
}
