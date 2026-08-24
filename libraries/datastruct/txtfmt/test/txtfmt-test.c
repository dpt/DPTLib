/* txtfmt-test.c -- txtfmt - text formatting */

#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "datastruct/txtfmt.h"

#include "test/all-tests.h"

static const char *data[] =
{
  "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Donec mattis luctus libero. Donec imperdiet, velit quis venenatis iaculis, metus libero cursus ligula, egestas sagittis dui diam in mi.",
  "The\nQuick\nBrown\nFox",
};

static const int widths[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29 };

static const int ndata = NELEMS(data);

result_t txtfmt_test(const char *resources)
{
  result_t   err;
  txtfmt_t  *tx[NELEMS(data)];
  int        i;

  NOT_USED(resources);

  printf("test: create\n");

  for (i = 0; i < ndata; i++)
  {
    err = txtfmt_create(data[i], &tx[i]);
    if (err)
      goto Failure;
  }

  printf("test: wrap, print, get-line\n");

  for (i = 0; i < ndata; i++)
  {
    int j;

    for (j = 0; j < NELEMS(widths); j++)
    {
      int k;
      int w = widths[j];

      printf("test string %d, wrapping to %d:\n", i, w);

      err = txtfmt_wrap(tx[i], w);
      if (err)
        goto Failure;

      printf("nlines=%d wrapped_width=%d\n",
             txtfmt_get_nlines(tx[i]), txtfmt_get_wrapped_width(tx[i]));

      err = txtfmt_print(tx[i]);
      if (err)
        goto Failure;

      for (k = 0; k < txtfmt_get_nlines(tx[i]); k++)
      {
        const char *line;
        int         length;

        err = txtfmt_get_line(tx[i], k, &line, &length);
        if (err)
          goto Failure;
      }

      err = txtfmt_get_line(tx[i], txtfmt_get_nlines(tx[i]), NULL, NULL);
      if (err != result_BAD_ARG)
        goto Failure;
    }
  }

  printf("test: destroy\n");

  for (i = 0; i < ndata; i++)
    txtfmt_destroy(tx[i]);

  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
