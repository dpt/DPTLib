/* stream-mem.c -- memory block IO stream implementation */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"
#include "base/utils.h"

#include "io/stream.h"

#include "io/stream-mem.h"

typedef struct stream_mem
{
  stream_t             base;

  const unsigned char *block;
  stream_size_t        length;
}
stream_mem_t;

static result_t stream_mem_seek(stream_t *s, stream_size_t pos)
{
  stream_mem_t *sm = (stream_mem_t *) s;

  if (pos > sm->length) /* allow seeks equal to file length (to EOF) */
    return result_STREAM_BAD_SEEK;

  sm->base.buf = sm->block + pos;

  return result_OK;
}

static int stream_mem_get(stream_t *s)
{
  stream_mem_t *sm = (stream_mem_t *) s;

  NOT_USED(sm); /* only used in debug builds */

  /* are we only called when buffer empty? */
  assert(sm->base.buf == sm->base.end);

  return EOF;
}

static stream_size_t stream_mem_fill(stream_t *s)
{
  return stream_remaining(s); /* no refill */
}

static stream_size_t stream_mem_length(stream_t *s)
{
  stream_mem_t *sm = (stream_mem_t *) s;

  return sm->length;
}

result_t stream_mem_create(const unsigned char *block,
                           stream_size_t        length,
                           stream_t           **s)
{
  stream_mem_t *sm;

  assert(block);

  sm = malloc(sizeof(*sm));
  if (!sm)
    return result_OOM;

  sm->base.buf     = block;
  sm->base.end     = block + length;

  sm->base.last    = result_OK;

  sm->base.op      = NULL;
  sm->base.seek    = stream_mem_seek;
  sm->base.get     = stream_mem_get;
  sm->base.fill    = stream_mem_fill;
  sm->base.length  = stream_mem_length;
  sm->base.destroy = NULL;

  sm->block  = block;
  sm->length = length;

  *s = &sm->base;

  return result_OK;
}
