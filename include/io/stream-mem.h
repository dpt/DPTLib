/* stream-mem.c -- memory block IO stream implementation */

#ifndef STREAM_MEM_H
#define STREAM_MEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "base/result.h"

#include "io/stream.h"

/**
 * Create a stream from a block of memory.
 *
 * \param[in]  block  Block of memory to create the stream from.
 * \param[in]  length Length of the block of memory in bytes.
 * \param[out] s      Pointer to a `stream_t` pointer to store the created
 *                    stream.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t stream_mem_create(const unsigned char *block,
                           size_t               length,
                           stream_t           **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_MEM_H */
