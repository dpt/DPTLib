/* framebuf/bitmap/save.c */

#include <assert.h>
#include <stdlib.h>

#include <png.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

result_t bitmap_save_png(const bitmap_t *bm, const char *filename)
{
#ifdef DPTLIB_IMAGES_READ_ONLY
  return result_NOT_SUPPORTED;
#else
  result_t             rc;
  volatile pixelfmt_t  fmt;
  volatile size_t      bytespp;
  volatile unsigned int shr, shg, shb, sha; /* channel bit positions */
  volatile int          paletted;
  volatile int          bitdepth;
  FILE                 *fp;
  png_structp           png_ptr  = NULL;
  png_infop volatile   info_ptr = NULL;
  png_bytep volatile   outrow   = NULL;
  png_colorp volatile  png_plte = NULL;
  const pixelfmt_xxxa8888_t *inrow; /* more like xxxx8888 */
  int                        x,y;

  paletted = 0;
  bitdepth = 8;

  switch (bm->format)
  {
  case pixelfmt_rgbx8888:
  case pixelfmt_bgrx8888:
  case pixelfmt_xbgr8888:
  case pixelfmt_xrgb8888:
    fmt     = PNG_COLOR_TYPE_RGB;
    bytespp = 3;
    break;
  case pixelfmt_rgba8888:
  case pixelfmt_bgra8888:
  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
    fmt     = PNG_COLOR_TYPE_RGBA;
    bytespp = 4;
    break;
  case pixelfmt_p1:
  case pixelfmt_p2:
  case pixelfmt_p4:
  case pixelfmt_p8:
    if (bm->palette == NULL)
      return result_NOT_SUPPORTED;
    fmt      = PNG_COLOR_TYPE_PALETTE;
    bytespp  = 1;
    paletted = 1;
    bitdepth = 1 << pixelfmt_log2bpp(bm->format);
    break;
  default:
    return result_NOT_SUPPORTED;
  }

  switch (bm->format)
  {
  case pixelfmt_rgbx8888:
  case pixelfmt_rgba8888:
    shr = 0;  shg = 8;  shb = 16; sha = 24;
    break;
  case pixelfmt_bgrx8888:
  case pixelfmt_bgra8888:
    shr = 16; shg = 8;  shb = 0;  sha = 24;
    break;
  case pixelfmt_xbgr8888:
  case pixelfmt_abgr8888:
    shr = 24; shg = 16; shb = 8;  sha = 0;
    break;
  case pixelfmt_xrgb8888:
  case pixelfmt_argb8888:
    shr = 8;  shg = 16; shb = 24; sha = 0;
    break;
  default: /* paletted */
    shr = shg = shb = sha = 0;
    break;
  }

  fp = fopen(filename, "wb");
  if (fp == NULL)
  {
    rc = result_FOPEN_FAILED;
    goto cleanup;
  }

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  if (setjmp(png_jmpbuf(png_ptr)))
  {
    rc = result_BAD_ARG; /* not ideal */
    goto cleanup;
  }

  png_init_io(png_ptr, fp);

  png_set_IHDR(png_ptr, info_ptr,
               bm->size.w, bm->size.h,
               bitdepth,
               fmt,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  if (paletted)
  {
    png_byte trns[256];
    int      have_trns;
    int      nplte;
    int      i;

    nplte    = 1 << bitdepth;
    png_plte = png_malloc(png_ptr, nplte * sizeof(*png_plte));
    if (png_plte == NULL)
    {
      rc = result_OOM;
      goto cleanup;
    }

    have_trns = 0;
    for (i = 0; i < nplte; i++)
    {
      pixelfmt_rgba8888_t px = bm->palette[i].primary;

      png_plte[i].red   = PIXELFMT_Rxxx8888(px);
      png_plte[i].green = PIXELFMT_xGxx8888(px);
      png_plte[i].blue  = PIXELFMT_xxBx8888(px);

      trns[i] = PIXELFMT_xxxA8888(px);
      if (trns[i] != 0xFF)
        have_trns = 1;
    }

    png_set_PLTE(png_ptr, info_ptr, png_plte, nplte);
    if (have_trns)
      png_set_tRNS(png_ptr, info_ptr, trns, nplte, NULL);
  }

  png_write_info(png_ptr, info_ptr);

  // consider:
  // png_write_image(png_ptr, row_pointers);
  // png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
  // png_set_packing for <8bpp images

  outrow = malloc(bytespp * bm->size.w * sizeof(png_byte));
  if (outrow == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  if (paletted)
  {
    const unsigned char *srcrow = bm->base;

    /* p4 packs the leftmost pixel in the low nibble, PNG wants it in the
     * high one. p1 and p2 already put it in the high bits, as PNG does. */
    if (bitdepth == 4)
      png_set_packswap(png_ptr);

    for (y = 0; y < bm->size.h; y++)
    {
      png_write_row(png_ptr, srcrow);
      srcrow += bm->rowbytes;
    }

    png_write_end(png_ptr, NULL);
    rc = result_OK;
    goto cleanup;
  }

  for (y = 0; y < bm->size.h; y++)
  {
    png_bytep pout = &outrow[0];

    inrow = (const pixelfmt_xxxa8888_t *) ((const char *) bm->base +
                                           (size_t) y * bm->rowbytes);

    for (x = 0; x < bm->size.w; x++)
    {
      pixelfmt_xxxa8888_t in = *inrow++;

      *pout++ = (png_byte) (in >> shr);
      *pout++ = (png_byte) (in >> shg);
      *pout++ = (png_byte) (in >> shb);
      if (bytespp == 4)
        *pout++ = (png_byte) (in >> sha);
    }

    png_write_row(png_ptr, outrow);
  }

  png_write_end(png_ptr, NULL);

  rc = result_OK;

cleanup:
  if (fp)
    fclose(fp);
  if (png_plte != NULL)
    png_free(png_ptr, png_plte);
  png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  png_destroy_write_struct(&png_ptr, NULL);
  free(outrow);

  return rc;
#endif /* DPTLIB_IMAGES_READ_ONLY */
}

/* vim: set ts=8 sts=2 sw=2 et: */
