/* load.c */

#include <assert.h>
#include <stdlib.h>

#include <png.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

static int file_is_png(FILE *fp)
{
#define PNGSIGLEN (8)
  unsigned char buf[PNGSIGLEN];

  if (fseek(fp, 0, SEEK_SET) < 0)
    return 0;

  if (fread(buf, 1, PNGSIGLEN, fp) != PNGSIGLEN)
    return 0;

  return png_sig_cmp(&buf[0], 0, PNGSIGLEN) == 0;
}

result_t bitmap_load_png(bitmap_t *bm, const char *filename)
{
  result_t       rc;
  FILE          *fp;
  png_structp    png_ptr      = NULL;
  png_infop      info_ptr;
  png_uint_32    pngwidth, pngheight;
  int            pngbitdepth, pngcolourtype;
  unsigned char *pixels       = NULL;
  png_bytep     *row_pointers = NULL;
  pixelfmt_t     bm_fmt;
  int            bm_rowbytes;
  png_uint_32    h;

  fp = fopen(filename, "rb");
  if (fp == NULL)
  {
    rc = result_FILE_NOT_FOUND;
    goto cleanup;
  }

  if (!file_is_png(fp))
  {
    rc = result_BAD_ARG;
    goto cleanup;
  }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
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
    rc = result_BAD_ARG;
    goto cleanup;
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, PNGSIGLEN);
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr,
              &pngwidth, &pngheight, &pngbitdepth, &pngcolourtype,
               NULL, NULL, NULL);

  if (pngbitdepth != 8)
  {
    rc = result_INCOMPATIBLE;
    goto cleanup;
  }

  switch (pngcolourtype)
  {
  case PNG_COLOR_TYPE_RGB:
    bm_fmt = pixelfmt_rgbx8888;
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    break;
  case PNG_COLOR_TYPE_RGBA:
    bm_fmt = pixelfmt_rgba8888;
    break;
  }

  bm_rowbytes = (pngwidth << pixelfmt_log2bpp(bm_fmt)) >> 3;

  pixels = malloc(bm_rowbytes * pngheight);
  if (pixels == NULL)
    goto oom;

  row_pointers = malloc(pngheight * sizeof(*row_pointers));
  if (row_pointers == NULL)
    goto oom;

  for (h = 0; h < pngheight; h++)
    row_pointers[h] = pixels + bm_rowbytes * h;

  png_read_image(png_ptr, row_pointers);

  bitmap_init(bm,
              pngwidth, pngheight,
              bm_fmt,
              bm_rowbytes,
              NULL, /* no palette */
              pixels);

  pixels = NULL; /* belongs to bitmap now */

  rc = result_OK;
  goto cleanup;

oom:
  rc = result_OOM;
  goto cleanup;

cleanup:
  free(row_pointers);
  free(pixels);
  if (png_ptr)
    png_destroy_read_struct(&png_ptr, NULL, NULL);
  fclose(fp);
  return rc;
}

/* vim: set ts=8 sts=2 sw=2 et: */
