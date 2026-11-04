/* text/utf8/word.c -- step over words separated by spaces and newlines */

#include <assert.h>

#include "text/utf8.h"

#define UTF8_IS_SEPARATOR(c) ((c) == ' ' || (c) == '\n')

int utf8_word_left(const char *s, int index)
{
  assert(s);

  while (index > 0 && UTF8_IS_SEPARATOR(s[index - 1]))
    index--;
  while (index > 0 && !UTF8_IS_SEPARATOR(s[index - 1]))
    index--;

  return index;
}

int utf8_word_right(const char *s, int len, int index)
{
  assert(s);

  while (index < len && UTF8_IS_SEPARATOR(s[index]))
    index++;
  while (index < len && !UTF8_IS_SEPARATOR(s[index]))
    index++;

  return index;
}
