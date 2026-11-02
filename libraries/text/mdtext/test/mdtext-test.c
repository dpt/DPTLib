/* text/mdtext/test/mdtext-test.c -- Markdown layout and draw */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "io/path.h"

#include "text/mdtext.h"

#include "test/all-tests.h"

#define W 160
#define H 120

/* ----------------------------------------------------------------------- */

static int count_inked(const unsigned int *pixels, int y0, int y1)
{
  int n, i;

  n = 0;
  for (i = y0 * W; i < y1 * W; i++)
    n += pixels[i] != 0;

  return n;
}

result_t mdtext_test(const char *resources)
{
  static const char plain[] = "one\n\nthree";
  static const char md[]    = "# Head\n\nSome **bold** words that wrap "
                              "over several lines at this width.";

  result_t      rc;
  char          dir[DPTLIB_MAXPATH];
  bmfont_t     *font;
  bmfont_t     *bold;
  mdtext_t      mdtext;
  unsigned int *pixels;
  screen_t      scr;
  int           ok;
  int           lh;

  font   = NULL;
  bold   = NULL;
  pixels = NULL;
  memset(&mdtext, 0, sizeof(mdtext));

  snprintf(dir, sizeof(dir), "%s", pathf("%s/resources/bmfonts/GrongyUI",
                                         resources));
  rc = bmfont_create(pathf("%s/Regular.png", dir), &font);
  if (rc != result_OK)
    goto exit;
  rc = bmfont_create(pathf("%s/Bold.png", dir), &bold);
  if (rc != result_OK)
    goto exit;

  mdtext.font    = font;
  mdtext.bold    = bold;
  mdtext.leading = 2;

  rc = result_TEST_FAILED;

  /* plain text: one line per newline, the blank one kept */
  if (mdtext_set(&mdtext, plain, (int) strlen(plain), 0) != result_OK ||
      mdtext_layout(&mdtext, W) != result_OK ||
      mdtext_nlines(&mdtext) != 3 ||
      mdtext.nspans != 0)
    goto exit;

  /* Markdown: markers stripped, spans found, long lines wrapped */
  if (mdtext_set(&mdtext, md, (int) strlen(md), 1) != result_OK ||
      mdtext_layout(&mdtext, W) != result_OK ||
      mdtext.nspans < 2 ||
      strncmp(mdtext.text, "Head", 4) != 0 ||
      mdtext_nlines(&mdtext) < 4)
    goto exit;

  /* right alignment indents a short line by the space left over */
  mdtext.align = mdtext_ALIGN_RIGHT;
  if (mdtext_layout(&mdtext, W) != result_OK || mdtext.dx[0] <= 0 ||
      mdtext.dx[1] != 0)
    goto exit;

  /* drawing inks the first line but nothing below the clip */
  pixels = calloc(W * H, sizeof(*pixels));
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto exit;
  }
  screen_init(&scr, SIZE2D(W, H), pixelfmt_rgbx8888, W * 4, NULL, pixels);
  lh       = mdtext_line_height(&mdtext);
  scr.clip = (box_t) BOX_POS_SIZE(0, 0, W, 2 * lh);
  mdtext_draw(&mdtext, &scr, POINT(0, 0), colour_rgb(0xFF, 0xFF, 0xFF),
              colour_rgba(0, 0, 0, 0));

  ok = count_inked(pixels, 0, lh) > 0 && count_inked(pixels, 2 * lh, H) == 0;
  rc = ok ? result_TEST_PASSED : result_TEST_FAILED;

exit:
  free(pixels);
  mdtext_free(&mdtext);
  bmfont_destroy(bold);
  bmfont_destroy(font);
  return rc;
}
