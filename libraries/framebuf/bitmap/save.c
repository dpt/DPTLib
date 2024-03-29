/* save.c */

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
  pixelfmt_t           fmt;
  size_t               bytespp;
  FILE                *fp;
  png_structp          png_ptr  = NULL;
  png_infop            info_ptr = NULL;
  png_bytep            outrow   = NULL;
  pixelfmt_xxxa8888_t *inrow; // more like xxxx8888
  int                  x,y;

  switch (bm->format)
  {
  case pixelfmt_bgrx8888:
    fmt     = PNG_COLOR_TYPE_RGB;
    bytespp = 3;
    break;
  case pixelfmt_bgra8888:
    fmt     = PNG_COLOR_TYPE_RGBA;
    bytespp = 4;
    break;
  default:
    return result_NOT_SUPPORTED;
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
               bm->width, bm->height,
               8,
               fmt,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr, info_ptr);

  // consider:
  // png_write_image(png_ptr, row_pointers);
  // png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
  // png_set_packing for <8bpp images

  outrow = malloc(bytespp * bm->width * sizeof(png_byte));
  if (outrow == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  inrow = bm->base;

  for (y = 0; y < bm->height; y++)
  {
    png_bytep pout = &outrow[0];

    switch (bm->format)
    {
    case pixelfmt_bgrx8888:
      for (x = 0; x < bm->width; x++)
      {
        pixelfmt_xxxa8888_t in = *inrow++;
        *pout++ = PIXELFMT_xxRx8888(in);
        *pout++ = PIXELFMT_xGxx8888(in);
        *pout++ = PIXELFMT_Bxxx8888(in);
      }
      break;

    case pixelfmt_bgra8888:
      for (x = 0; x < bm->width; x++)
      {
        pixelfmt_xxxa8888_t in = *inrow++;
        *pout++ = PIXELFMT_xxRx8888(in);
        *pout++ = PIXELFMT_xGxx8888(in);
        *pout++ = PIXELFMT_Bxxx8888(in);
        *pout++ = PIXELFMT_xxxA8888(in);
      }
      break;

    default:
      assert(0);
    }

    png_write_row(png_ptr, outrow);
    //inrow += bm->rowbytes / 4;
  }

  png_write_end(png_ptr, NULL);

  rc = result_OK;

cleanup:
  fclose(fp);
  png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  png_destroy_write_struct(&png_ptr, NULL);
  free(outrow);

  return rc;
#endif /* DPTLIB_IMAGES_READ_ONLY */
}

/* vim: set ts=8 sts=2 sw=2 et: */
