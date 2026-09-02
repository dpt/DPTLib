/* framebuf/bitmap/bitmap.c */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "framebuf/bitmap.h"
#include "framebuf/span-registry.h"

result_t bitmap_init(bitmap_t       *bm,
                     size2d_t        size,
                     pixelfmt_t      fmt,
                     int             rowbytes,
                     const colour_t *palette,
                     void           *base)
{
  assert(bm);

  bm->size     = size;
  bm->format   = fmt;
  bm->rowbytes = rowbytes;
  bm->palette  = NULL;
  bm->span     = spanregistry_get(fmt);
  bm->base     = base;

  return bitmap_set_palette(bm, palette);
}

result_t bitmap_set_palette(bitmap_t *bm, const colour_t *palette)
{
  int       log2bpp;
  int       nentries;
  colour_t *pal;

  assert(bm);

  log2bpp = pixelfmt_log2bpp(bm->format);
  if (log2bpp > 3)
    return result_OK; /* no palette for this format */

  if (palette == NULL)
  {
    free(bm->palette);
    bm->palette = NULL;
    return result_OK;
  }

  nentries = 1 << (1 << log2bpp);

  /* the entry count is fixed by the format, so an existing buffer is always
   * the right size to reuse */
  pal = bm->palette;
  if (pal == NULL)
  {
    pal = malloc(nentries * sizeof(*pal));
    if (pal == NULL)
      return result_OOM;
  }

  memcpy(pal, palette, nentries * sizeof(*pal));
  bm->palette = pal;

  return result_OK;
}

void bitmap_clear(bitmap_t *bm, colour_t colour)
{
  int            log2bpp;
  pixelfmt_any_t px;
  int            x,y;

  assert(bm);

  log2bpp = pixelfmt_log2bpp(bm->format);
  px = colour_to_pixel(bm->palette,
                       bm->palette ? 1 << (1 << log2bpp) : 0,
                       colour,
                       bm->format);

  switch (log2bpp)
  {
  case 0: /* 1bpp */
  case 1: /* 2bpp */
  case 2: /* 4bpp */
  case 3: /* 8bpp */
    switch (log2bpp)
    {
    case 0: px *= 0xFF; break;
    case 1: px *= 0x55; break;
    case 2: px *= 0x11; break;
    case 3: px *= 0x01; break;
    }
    memset(bm->base, px, bm->rowbytes * bm->size.h);
    break;

  case 5: /* 32bpp - pixels are ints */
  {
    /* if all bytes of 'px' are the same, use memset() */
    pixelfmt_any32_t tmp1 = px ^ (px >> 16);
    pixelfmt_any32_t tmp2 = tmp1 ^ (tmp1 >> 8);
    if (tmp2 == 0)
    {
      memset(bm->base, px, bm->rowbytes * bm->size.h);
    }
    else
    {
      pixelfmt_any32_t *pixels;

      pixels = bm->base;
      for (y = 0; y < bm->size.h; y++)
      {
        for (x = 0; x < bm->size.w; x++)
          *pixels++ = px;
        pixels += bm->rowbytes / sizeof(*pixels) - bm->size.w;
      }
    }
  }
    break;

  default:
    assert(0);
    return; /* not implemented */
  }
}

static result_t bmconv_p4_to_bgrx8888(const bitmap_t *src, bitmap_t **pdst)
{
  result_t             rc;
  bitmap_t            *dst;
  pixelfmt_bgrx8888_t *outpixels;
  pixelfmt_bgrx8888_t  map[16];
  int                  i;
  pixelfmt_p4_t       *inpixels;
  int                  x,y;

  assert(src);
  assert(src->palette);

  for (i = 0; i < 16; i++)
    map[i] = colour_to_pixel(src->palette, 16, src->palette[i], pixelfmt_bgrx8888);

  outpixels = malloc(src->size.w * sizeof(pixelfmt_bgrx8888_t) * src->size.h); // rowbytes rounding needed?
  if (outpixels == NULL)
    return result_OOM;

  dst = malloc(sizeof(*dst));
  if (dst == NULL)
  {
    free(outpixels);
    return result_OOM;
  }

  rc = bitmap_init(dst,
                   src->size,
                   pixelfmt_bgrx8888,
                   src->size.w * sizeof(pixelfmt_bgrx8888_t),
                   NULL,
                   outpixels);
  if (rc)
    return rc;

  inpixels = src->base;
  for (y = 0; y < src->size.h; y++)
  {
    for (x = 0; x < src->size.w / 8; x++)
    {
      pixelfmt_p4_t in = *inpixels++; // fetches 8 pixels
      // 0xABCDEFGH is 8 4bpp pixels shown H,G,F,E,D,C,B,A
      *outpixels++ = map[(in >>  0) & 0xF];
      *outpixels++ = map[(in >>  4) & 0xF];
      *outpixels++ = map[(in >>  8) & 0xF];
      *outpixels++ = map[(in >> 12) & 0xF];
      *outpixels++ = map[(in >> 16) & 0xF];
      *outpixels++ = map[(in >> 20) & 0xF];
      *outpixels++ = map[(in >> 24) & 0xF];
      *outpixels++ = map[(in >> 28) & 0xF];
    }
  }

  *pdst = dst;

  return rc;
}

result_t bitmap_convert(const bitmap_t *src,
                        pixelfmt_t      newfmt,
                        bitmap_t      **dst)
{
  *dst = NULL;

  switch (src->format)
  {
  case pixelfmt_p4:
    switch (newfmt)
    {
    case pixelfmt_bgrx8888:
      return bmconv_p4_to_bgrx8888(src, dst);

    default:
      return result_NOT_SUPPORTED;
    }
    break;

  default:
    return result_NOT_SUPPORTED;
  }
}
