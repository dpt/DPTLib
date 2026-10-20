/* text/bmtext/test/bmtext-test.c -- word-wrap layout */

#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "text/bmtext.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Every byte is opaque->widths[0] pixels wide, except bytes inside the
 * "wide" run [wide_start, wide_end) of the string, which are widths[1] -- a
 * stand-in for a bold run drawn in a wider face. */
typedef struct fake_measure
{
  const char *string;
  int         wide_start;
  int         wide_end;
  int         widths[2];
}
fake_measure_t;

static result_t fake_measure(const char     *text,
                             int             len,
                             bmfont_width_t  target_width,
                             int            *split_point,
                             bmfont_width_t *actual_width,
                             void           *opaque)
{
  fake_measure_t *fm = opaque;
  int             off;
  int             i;
  int             width;
  int             w;

  off   = (int) (text - fm->string);
  width = 0;
  for (i = 0; i < len; i++)
  {
    w = (off + i >= fm->wide_start && off + i < fm->wide_end) ? fm->widths[1]
                                                               : fm->widths[0];
    if (width + w > target_width)
      break;
    width += w;
  }

  *split_point  = i;
  *actual_width = width;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t bmtext_test(const char *resources)
{
  static const struct
  {
    const char *s;
    int         wide_start;
    int         wide_end;
    int         wrap_width;
    const char *expected; /* lines joined with '|' */
  }
  tests[] =
  {
    { "aaa bbb ccc", 0, 0,  7, "aaa bbb|ccc"   }, /* break at last space */
    { "aaa  bbb",    0, 0,  4, "aaa|bbb"       }, /* spaces swallowed */
    { "abcdefgh",    0, 0,  3, "abc|def|gh"    }, /* no space: hard break */
    { "aaa bbb ccc", 4, 7,  7, "aaa|bbb|ccc"   }, /* wide bbb pushes breaks */
    { "aaa bbb",     0, 0,  3, "aaa|bbb"       }, /* word ends at the edge */
    { "aaa bbb ccc", 4, 7, 10, "aaa bbb|ccc"   }, /* wide bbb still fits */
    { "",            0, 0,  5, ""              },
  };

  int            i;
  fake_measure_t fm;
  bmtext_line_t  lines[8];
  int            nlines;
  char           joined[64];
  int            j;

  NOT_USED(resources);

  for (i = 0; i < NELEMS(tests); i++)
  {
    fm.string     = tests[i].s;
    fm.wide_start = tests[i].wide_start;
    fm.wide_end   = tests[i].wide_end;
    fm.widths[0]  = 1;
    fm.widths[1]  = 2;

    nlines = bmtext_layout_measured(fake_measure, &fm, tests[i].s,
                                    (int) strlen(tests[i].s),
                                    tests[i].wrap_width, lines,
                                    NELEMS(lines));

    joined[0] = '\0';
    for (j = 0; j < nlines; j++)
    {
      if (j > 0)
        strcat(joined, "|");
      strncat(joined, lines[j].str, (size_t) lines[j].len);
    }

    if (strcmp(joined, tests[i].expected) != 0)
    {
      fprintf(stderr, "error: bmtext case %d laid out \"%s\", expected \"%s\"\n",
              i, joined, tests[i].expected);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}
