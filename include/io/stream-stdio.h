/* stream-stdio.c -- C standard IO stream implementation */

#ifndef STREAM_STDIO_H
#define STREAM_STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#include "base/result.h"

#include "io/stream.h"

/* use 0 for a sensible default buffer size */

result_t stream_stdio_create(FILE *f, int bufsz, stream_t **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_STDIO_H */
