/* framebuf/palettes/test/palettes-test.c */

#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "test/tmpdir.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Write "text" to a scratch file and load it into a 4-entry palette. */
static result_t palettes_test_load(const char *text,
                                   colour_t    palette[4],
                                   int        *n)
{
  result_t rc;
  char     path[256];
  FILE    *fp;

  if (!test_tmppath(path, sizeof(path), "palettes-test.hex"))
    return result_TEST_FAILED;

  fp = fopen(path, "wb");
  if (fp == NULL)
    return result_TEST_FAILED;
  fputs(text, fp);
  fclose(fp);

  rc = palettes_load_hex(path, palette, 4, n);
  remove(path);

  return rc;
}

result_t palettes_test(const char *resources)
{
  colour_t palette[4];
  int      n;

  NOT_USED(resources);

  /* CRLF endings, a blank line and short and '#' forms all load */
  n = 0;
  if (palettes_test_load("ff0000\r\n\r\n#00FF00\r\n00f\r\n", palette, &n) !=
        result_OK ||
      n != 3 ||
      palette[0].primary != colour_rgb(0xFF, 0x00, 0x00).primary ||
      palette[1].primary != colour_rgb(0x00, 0xFF, 0x00).primary ||
      palette[2].primary != colour_rgb(0x00, 0x00, 0xFF).primary)
    return result_TEST_FAILED;

  /* bad lines, too many entries and empty files are rejected, n untouched */
  n = -1;
  if (palettes_test_load("ff0000\nnot hex\n", palette, &n) != result_BAD_ARG ||
      palettes_test_load("000000\n111111\n222222\n333333\n444444\n", palette,
                         &n) != result_BAD_ARG ||
      palettes_test_load("\n", palette, &n) != result_BAD_ARG ||
      n != -1)
    return result_TEST_FAILED;

  if (palettes_load_hex("no/such/palette.hex", palette, 4, &n) !=
      result_FILE_NOT_FOUND)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}
