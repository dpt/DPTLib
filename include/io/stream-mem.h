/* --------------------------------------------------------------------------
 *    Name: stream-mem.h
 * Purpose: Memory block IO stream implementation
 * ----------------------------------------------------------------------- */

#ifndef STREAM_MEM_H
#define STREAM_MEM_H

#include <stddef.h>

#include "base/result.h"

#include "io/stream.h"

result_t stream_mem_create(const unsigned char *block,
                            size_t               length,
                            stream_t           **s);

#endif /* STREAM_MEM_H */
