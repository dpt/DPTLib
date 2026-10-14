/* wuss/copy-text.c -- wuss - minimal window manager */

#include <string.h>

#include "base/utils.h"

#include "types.h"
#include "windows.h"

void wuss__writable_copy(char *buf, int size, const char *text)
{
  size_t len;
  int    trimmed;

  len = MIN(strlen(text), (size_t) size - 1);

  /* don't split a codepoint: back off over up to three continuation bytes */
  for (trimmed = 0;
       trimmed < 3 && len > 0 && ((unsigned char) text[len] & 0xC0) == 0x80;
       trimmed++)
    len--;

  memcpy(buf, text, len);
  buf[len] = '\0';
}
