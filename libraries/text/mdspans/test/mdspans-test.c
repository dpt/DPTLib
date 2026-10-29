/* text/mdspans/test/mdspans-test.c -- minimal Markdown parser */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "text/mdspans.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

typedef struct mdspans_case
{
  const char     *md;
  int             maxspans;
  const char     *text;
  int             nspans;
  mdspan_t        spans[4];
}
mdspans_case_t;

static const mdspans_case_t cases[] =
{
  { "# Title\nbody", 4, "Title\nbody", 1,
    { { 0, 5, mdspans_HEADER } } },
  { "a **b** *c* _d_ `e`", 4, "a b c d e", 4,
    { { 2, 1, mdspans_BOLD }, { 4, 1, mdspans_ITALIC },
      { 6, 1, mdspans_ITALIC }, { 8, 1, mdspans_CODE } } },
  { "####### x", 4, "####### x", 0, { { 0 } } },
  /* unclosed on the line, or not a header: kept as is */
  { "*a\nb* #x ####### y", 4, "*a\nb* #x ####### y", 0, { { 0 } } },
  /* inline markers in a header are stripped, not styled */
  { "## A **b**", 4, "A b", 1, { { 0, 3, mdspans_HEADER } } },
  /* spans past maxspans are dropped, their markers still stripped */
  { "`a` `b`", 1, "a b", 1, { { 0, 1, mdspans_CODE } } },
};

result_t mdspans_test(const char *resources)
{
  size_t    i;
  char     *text;
  mdspan_t *spans;
  int       nspans;
  int       j;
  int       ok;

  NOT_USED(resources);

  for (i = 0; i < NELEMS(cases); i++)
  {
    if (mdspans_parse(cases[i].md, (int) strlen(cases[i].md),
                      cases[i].maxspans, &text, &spans, &nspans) != result_OK)
      return result_TEST_FAILED;

    ok = strcmp(text, cases[i].text) == 0 && nspans == cases[i].nspans;
    for (j = 0; ok && j < nspans && j < (int) NELEMS(cases[i].spans); j++)
      ok = spans[j].start == cases[i].spans[j].start &&
           spans[j].len   == cases[i].spans[j].len   &&
           spans[j].style == cases[i].spans[j].style;

    free(text);
    free(spans);

    if (!ok)
    {
      printf("mdspans: case %d failed\n", (int) i);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}
