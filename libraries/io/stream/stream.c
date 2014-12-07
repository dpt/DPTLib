/* stream.c -- stream system support functions */

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "io/stream.h"

result_t stream_op(stream_t *s, stream_opcode_t opcode, void *opaque)
{
  if (!s->op)
    return result_STREAM_UNKNOWN_OP;

  return s->op(s, opcode, opaque);
}

result_t stream_seek(stream_t *s, stream_size_t pos)
{
  if (!s->seek)
    return result_STREAM_CANT_SEEK;

  return s->seek(s, pos);
}

stream_size_t stream_length(stream_t *s)
{
  if (!s->length)
    return -1; // FIXME: Returned as size_t

  return s->length(s);
}

void stream_destroy(stream_t *doomed)
{
  if (!doomed)
    return;

  if (doomed->destroy)
    doomed->destroy(doomed);

  free(doomed);
}
