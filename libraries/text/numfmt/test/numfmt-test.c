/* text/numfmt/test/numfmt-test.c -- number formatting */

#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "text/numfmt.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

result_t numfmt_test(const char *resources)
{
  static const struct
  {
    unsigned long long value;
    const char        *expected;
  }
  cases[] =
  {
    { 0ULL,                    "0"                          },
    { 7ULL,                    "7"                          },
    { 999ULL,                  "999"                        },
    { 1000ULL,                 "1,000"                      },
    { 12345ULL,                "12,345"                     },
    { 100000ULL,               "100,000"                    },
    { 1000000ULL,              "1,000,000"                  },
    { 1234567890ULL,           "1,234,567,890"              },
    { 18446744073709551615ULL, "18,446,744,073,709,551,615" },
  };

  char buf[numfmt_GROUPED_MAX];
  int  i;

  NOT_USED(resources);

  for (i = 0; i < (int) NELEMS(cases); i++)
  {
    if (numfmt_grouped(cases[i].value, buf) != buf ||
        strcmp(buf, cases[i].expected) != 0)
    {
      printf("numfmt_grouped: got '%s', expected '%s'\n",
             buf, cases[i].expected);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}
