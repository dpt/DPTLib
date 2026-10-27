/* text/utf8/word.c -- step over space-separated words */

#include <assert.h>

#include "text/utf8.h"

int utf8_word_left(const char *s, int index)
{
  assert(s);

  while (index > 0 && s[index - 1] == ' ')
    index--;
  while (index > 0 && s[index - 1] != ' ')
    index--;

  return index;
}

int utf8_word_right(const char *s, int len, int index)
{
  assert(s);

  while (index < len && s[index] == ' ')
    index++;
  while (index < len && s[index] != ' ')
    index++;

  return index;
}
