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

result_t stream_mem_create(const unsigned char *block,
                           size_t               length,
                           stream_t           **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_MEM_H */
