/* framebuf/bitmap/load-jpeg.c -- JPEG loader */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

/* ----------------------------------------------------------------------- */

/* libjpeg's default error_exit calls exit(): jump back to the loader instead */
typedef struct
{
  struct jpeg_error_mgr pub;
  jmp_buf               jmp;
}
jpeg_error_t;

static void jpeg_error_exit(j_common_ptr cinfo)
{
  longjmp(((jpeg_error_t *) cinfo->err)->jmp, 1);
}

/* keep warnings (e.g. "premature end of data") off stderr */
static void jpeg_output_message(j_common_ptr cinfo)
{
  (void) cinfo;
}

/* ----------------------------------------------------------------------- */

result_t bitmap_load_jpeg(bitmap_t *bm, const char *filename)
{
  result_t rc;
  FILE    *fp;
  struct jpeg_decompress_struct cinfo;
  jpeg_error_t                  jerr;
  unsigned char       *volatile pixels = NULL;
  int                  grey;
  pixelfmt_t           bm_fmt;
  int                  bm_rowbytes;
  JSAMPARRAY           row;
  unsigned char       *out;
  const unsigned char *in;
  JDIMENSION           x;
  colour_t             plte[256];
  int                  i;

  fp = fopen(filename, "rb");
  if (fp == NULL)
    return result_FILE_NOT_FOUND;

  cinfo.err               = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit     = jpeg_error_exit;
  jerr.pub.output_message = jpeg_output_message;
  jpeg_create_decompress(&cinfo);

  if (setjmp(jerr.jmp))
  {
    rc = result_BAD_ARG;
    goto cleanup;
  }

  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, TRUE);

  /* greyscale stays one byte per pixel, as p8 with a ramp of greys, as
   * bitmap_load_png does; colour is decoded as RGB and padded to rgbx8888.
   * CMYK and YCCK are not handled */
  grey = (cinfo.jpeg_color_space == JCS_GRAYSCALE);
  if (!grey && cinfo.jpeg_color_space != JCS_YCbCr &&
      cinfo.jpeg_color_space != JCS_RGB)
  {
    rc = result_INCOMPATIBLE;
    goto cleanup;
  }
  cinfo.out_color_space = grey ? JCS_GRAYSCALE : JCS_RGB;

  jpeg_start_decompress(&cinfo);

  bm_fmt      = grey ? pixelfmt_p8 : pixelfmt_rgbx8888;
  bm_rowbytes = (int) cinfo.output_width << (grey ? 0 : 2);

  pixels = malloc((size_t) bm_rowbytes * cinfo.output_height);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto cleanup;
  }

  /* freed by jpeg_destroy_decompress */
  row = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE,
                                   cinfo.output_width *
                                   cinfo.output_components, 1);

  while (cinfo.output_scanline < cinfo.output_height)
  {
    out = pixels + (size_t) bm_rowbytes * cinfo.output_scanline;
    jpeg_read_scanlines(&cinfo, row, 1);
    in = row[0];
    if (grey)
    {
      for (x = 0; x < cinfo.output_width; x++)
        out[x] = in[x];
    }
    else
    {
      for (x = 0; x < cinfo.output_width; x++)
      {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = 0xFF;
        out += 4;
        in  += 3;
      }
    }
  }

  jpeg_finish_decompress(&cinfo);

  if (grey)
    for (i = 0; i < 256; i++)
      plte[i] = colour_rgba(i, i, i, 0xFF);

  rc = bitmap_init(bm, SIZE2D(cinfo.output_width, cinfo.output_height),
                   bm_fmt,
                   bm_rowbytes,
                   grey ? plte : NULL, /* p8: copied in */
                   pixels);
  if (rc != result_OK)
    goto cleanup;

  pixels = NULL; /* belongs to bitmap now */

cleanup:
  free(pixels);
  jpeg_destroy_decompress(&cinfo);
  fclose(fp);
  return rc;
}

/* vim: set ts=8 sts=2 sw=2 et: */
