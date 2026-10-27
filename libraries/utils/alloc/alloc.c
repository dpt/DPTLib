/* utils/alloc/alloc.c -- pluggable allocator hooks */

#include <stdlib.h>
#include <string.h>

#include "utils/alloc.h"

const alloc_t alloc_default =
{
  malloc, realloc, free
};

char *alloc_strdup(const alloc_t *a, const char *s)
{
  size_t len;
  char  *copy;

  if (s == NULL)
    return NULL;

  len  = strlen(s) + 1;
  copy = a->malloc(len);
  if (copy != NULL)
    memcpy(copy, s, len);

  return copy;
}
