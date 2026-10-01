/* text/utf8/test/utf8-test.c -- UTF-8 decoding */

#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "text/utf8.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

result_t utf8_test(const char *resources)
{
  static const struct
  {
    const char   *s;
    int           len; /* 0 means strlen(s) */
    unsigned long codepoint;
    int           consumed;
  }
  tests[] =
  {
    { "A",                    0, 0x41,             1 },
    { "\x7F",                 0, 0x7F,             1 },
    { "\xC2\xA9",             0, 0xA9,             2 }, /* copyright */
    { "\xE2\x86\x90",         0, 0x2190,           3 }, /* left arrow */
    { "\xEF\xBF\xBD",         0, 0xFFFD,           3 }, /* literal U+FFFD */
    { "\xF0\x9F\x98\x80",     0, 0x1F600,          4 },
    { "\xF4\x8F\xBF\xBF",     0, 0x10FFFF,         4 }, /* highest */
    { "\x80",                 0, utf8_REPLACEMENT, 1 }, /* stray continuation */
    { "\xFF",                 0, utf8_REPLACEMENT, 1 },
    { "\xC0\x80",             0, utf8_REPLACEMENT, 1 }, /* overlong NUL */
    { "\xE0\x80\xAF",         0, utf8_REPLACEMENT, 1 }, /* overlong '/' */
    { "\xED\xA0\x80",         0, utf8_REPLACEMENT, 1 }, /* surrogate */
    { "\xF4\x90\x80\x80",     0, utf8_REPLACEMENT, 1 }, /* above U+10FFFF */
    { "\xC2" "A",             0, utf8_REPLACEMENT, 1 }, /* bad continuation */
    { "\xE2\x86\x90",         2, utf8_REPLACEMENT, 1 }, /* cut short by len */
  };

  int           i;
  int           len;
  unsigned long codepoint;
  int           consumed;
  char          buf[4];

  NOT_USED(resources);

  for (i = 0; i < NELEMS(tests); i++)
  {
    len      = tests[i].len ? tests[i].len : (int) strlen(tests[i].s);
    consumed = utf8_decode(tests[i].s, len, &codepoint);
    if (codepoint != tests[i].codepoint || consumed != tests[i].consumed)
    {
      fprintf(stderr,
              "error: utf8 case %d decoded U+%04lX/%d, expected U+%04lX/%d\n",
              i, codepoint, consumed, tests[i].codepoint, tests[i].consumed);
      return result_TEST_FAILED;
    }

    /* whatever decoded, stepping back from its end returns to its start */
    if (utf8_prev(tests[i].s, consumed) != 0)
    {
      fprintf(stderr, "error: utf8 case %d stepped back wrongly\n", i);
      return result_TEST_FAILED;
    }

    /* valid codepoints encode back to the bytes they came from */
    if (codepoint != utf8_REPLACEMENT || consumed == 3)
    {
      if (utf8_encode(codepoint, buf) != consumed ||
          memcmp(buf, tests[i].s, (size_t) consumed) != 0)
      {
        fprintf(stderr, "error: utf8 case %d didn't re-encode\n", i);
        return result_TEST_FAILED;
      }
    }
  }

  /* stepping back over a malformed byte takes one byte, even when it follows
   * a whole sequence */
  if (utf8_prev("\xC2\xA9\x80", 3) != 2 ||
      utf8_prev("\xC2\xA9\x80", 2) != 0 ||
      utf8_prev("\xE2\x86", 2) != 1)
  {
    fprintf(stderr, "error: utf8_prev over malformed bytes\n");
    return result_TEST_FAILED;
  }

  if (utf8_encode(0xD800, buf) != 0 || utf8_encode(0x110000, buf) != 0)
  {
    fprintf(stderr, "error: utf8_encode accepted an invalid codepoint\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}
