/* framebuf/bmfont/test/bmfontfamily-test.c */

#include <stdio.h>
#include <string.h>

#include "base/utils.h"
#include "framebuf/bmfontfamily.h"
#include "io/path.h"

#include "test/all-tests.h"

/* Fixture/ holds empty PNGs: only the names matter, nothing is loaded. */

static int style_is(const bmfontface_t *face, const char *style)
{
  return face != NULL && strcmp(face->style, style) == 0;
}

static result_t test_scan_order(const char *resources)
{
  result_t            rc;
  bmfontfamily_t     *family;
  const bmfontface_t *face;
  int                 i;
  static const char         *const expected[] =
  {
    "Light", "Regular", "Italic", "Bold", "BoldItalic", "Black"
  };

  rc = bmfontfamily_scan(pathf("%s/resources/bmfonts-test/Fixture",
                               resources), &family);
  if (rc != result_OK)
  {
    printf("bmfontfamily_scan: rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  if (strcmp(bmfontfamily_name(family), "Fixture") != 0 ||
      bmfontfamily_count(family) != (int) NELEMS(expected))
  {
    printf("bmfontfamily_scan: name \"%s\" count %d\n",
           bmfontfamily_name(family), bmfontfamily_count(family));
    bmfontfamily_destroy(family);
    return result_TEST_FAILED;
  }

  for (i = 0; i < (int) NELEMS(expected); i++)
  {
    face = bmfontfamily_face(family, i);
    if (!style_is(face, expected[i]) || strstr(face->path, ".png") == NULL)
    {
      printf("bmfontfamily_face(%d): expected %s\n", i, expected[i]);
      bmfontfamily_destroy(family);
      return result_TEST_FAILED;
    }
  }

  if (bmfontfamily_face(family, -1) != NULL ||
      bmfontfamily_face(family, (int) NELEMS(expected)) != NULL)
  {
    printf("bmfontfamily_face: out-of-range index not rejected\n");
    bmfontfamily_destroy(family);
    return result_TEST_FAILED;
  }

  bmfontfamily_destroy(family);

  return result_TEST_PASSED;
}

static result_t test_selection(const char *resources)
{
  result_t            rc;
  bmfontfamily_t     *family;
  const bmfontface_t *regular;
  const bmfontface_t *bold;
  const bmfontface_t *black;
  const bmfontface_t *light;
  const bmfontface_t *italic;
  const bmfontface_t *bold_italic;
  int                 ok;

  rc = bmfontfamily_scan(pathf("%s/resources/bmfonts-test/Fixture",
                               resources), &family);
  if (rc != result_OK)
  {
    printf("bmfontfamily_scan: rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  regular     = bmfontfamily_find(family, bmfontfamily_WEIGHT_REGULAR,
                                  bmfontfamily_SLANT_UPRIGHT);
  bold        = bmfontfamily_find(family, bmfontfamily_WEIGHT_BOLD,
                                  bmfontfamily_SLANT_UPRIGHT);
  black       = bmfontfamily_find(family, bmfontfamily_WEIGHT_BLACK,
                                  bmfontfamily_SLANT_UPRIGHT);
  light       = bmfontfamily_find(family, bmfontfamily_WEIGHT_LIGHT,
                                  bmfontfamily_SLANT_UPRIGHT);
  italic      = bmfontfamily_find(family, bmfontfamily_WEIGHT_REGULAR,
                                  bmfontfamily_SLANT_ITALIC);
  bold_italic = bmfontfamily_find(family, bmfontfamily_WEIGHT_BOLD,
                                  bmfontfamily_SLANT_ITALIC);

  ok = regular && bold && black && light && italic && bold_italic &&
       bmfontfamily_find(family, bmfontfamily_WEIGHT_THIN,
                         bmfontfamily_SLANT_UPRIGHT) == NULL;

  /* heavier steps up the same slant and stops at the end */
  ok = ok && bmfontfamily_heavier(family, regular) == bold;
  ok = ok && bmfontfamily_heavier(family, bold) == black;
  ok = ok && bmfontfamily_heavier(family, black) == black;
  ok = ok && bmfontfamily_heavier(family, italic) == bold_italic;
  ok = ok && bmfontfamily_heavier(family, bold_italic) == bold_italic;

  /* lighter likewise */
  ok = ok && bmfontfamily_lighter(family, bold) == regular;
  ok = ok && bmfontfamily_lighter(family, regular) == light;
  ok = ok && bmfontfamily_lighter(family, light) == light;
  ok = ok && bmfontfamily_lighter(family, bold_italic) == italic;

  /* with_slant keeps weight when it can, else takes the nearest */
  ok = ok && bmfontfamily_with_slant(family, bold,
                                     bmfontfamily_SLANT_ITALIC) == bold_italic;
  ok = ok && bmfontfamily_with_slant(family, black,
                                     bmfontfamily_SLANT_ITALIC) == bold_italic;
  ok = ok && bmfontfamily_with_slant(family, light,
                                     bmfontfamily_SLANT_ITALIC) == italic;
  ok = ok && bmfontfamily_with_slant(family, bold_italic,
                                     bmfontfamily_SLANT_UPRIGHT) == bold;
  ok = ok && bmfontfamily_with_slant(family, bold,
                                     bmfontfamily_SLANT_UPRIGHT) == bold;

  bmfontfamily_destroy(family);

  if (!ok)
  {
    printf("bmfontfamily: selection primitives gave a wrong face\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

static result_t test_real_family(const char *resources)
{
  result_t            rc;
  bmfontfamily_t     *family;
  const bmfontface_t *regular;
  const bmfontface_t *bold;

  rc = bmfontfamily_scan(pathf("%s/resources/bmfonts/GrongyUI", resources),
                         &family);
  if (rc != result_OK)
  {
    printf("bmfontfamily_scan(GrongyUI): rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  regular = bmfontfamily_find(family, bmfontfamily_WEIGHT_REGULAR,
                              bmfontfamily_SLANT_UPRIGHT);
  bold    = bmfontfamily_heavier(family, regular);

  if (bmfontfamily_count(family) != 2 || !style_is(bold, "Bold") ||
      bmfontfamily_with_slant(family, bold,
                              bmfontfamily_SLANT_ITALIC) != bold)
  {
    printf("bmfontfamily: GrongyUI is not Regular + Bold\n");
    bmfontfamily_destroy(family);
    return result_TEST_FAILED;
  }

  bmfontfamily_destroy(family);

  return result_TEST_PASSED;
}

static result_t test_errors(const char *resources)
{
  result_t        rc;
  bmfontfamily_t *family;

  (void) resources;

  rc = bmfontfamily_scan("no/such/dir/here", &family);
  if (rc != result_FILE_NOT_FOUND)
  {
    printf("bmfontfamily_scan bad dir: rc %x, expected %x\n", rc,
           result_FILE_NOT_FOUND);
    return result_TEST_FAILED;
  }

  if (bmfontfamily_scan(NULL, &family) != result_NULL_ARG ||
      bmfontfamily_scan("x", NULL) != result_NULL_ARG)
  {
    printf("bmfontfamily_scan: NULL arg not rejected\n");
    return result_TEST_FAILED;
  }

  bmfontfamily_destroy(NULL); /* must not crash */

  return result_TEST_PASSED;
}

result_t bmfontfamily_test(const char *resources)
{
  static result_t (*const tests[])(const char *) =
  {
    test_scan_order,
    test_selection,
    test_real_family,
    test_errors
  };

  result_t rc;
  size_t   i;
  int      nfailures;

  nfailures = 0;
  for (i = 0; i < NELEMS(tests); i++)
  {
    rc = tests[i](resources);
    if (rc != result_TEST_PASSED)
      nfailures++;
  }

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}
