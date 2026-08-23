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

/**
 * Create a stream from a C standard IO file.
 *
 * \param[in]  f      File to create the stream from.
 * \param[in]  bufsz  Buffer size in bytes (0 for a sensible default).
 * \param[out] s      Pointer to a `stream_t` pointer to store the created stream.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t stream_stdio_create(FILE *f, int bufsz, stream_t **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_STDIO_H */
