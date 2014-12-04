/* --------------------------------------------------------------------------
 *    Name: stream-stdio.h
 * Purpose: C standard IO stream implementation
 * ----------------------------------------------------------------------- */

#ifndef STREAM_STDIO_H
#define STREAM_STDIO_H

#include <stdio.h>

#include "base/result.h"

#include "io/stream.h"

/* use 0 for a sensible default buffer size */

result_t stream_stdio_create(FILE *f, int bufsz, stream_t **s);

#endif /* STREAM_STDIO_H */
