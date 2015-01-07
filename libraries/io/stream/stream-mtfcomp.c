/* stream-mtfcomp.c -- "Move to front" adaptive compression stream */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "io/stream.h"
#include "utils/array.h"

#include "io/stream-mtfcomp.h"

#define DBUG(args)

#define TABSZ   15  /* 16 being the escape character */
#define ESCAPE  TABSZ
#define DFLTTAB " etoasrinclmhdu"

typedef struct MTFState
{
  unsigned char tab[TABSZ];
  unsigned int  buf;  /* longest buffered sequence */
  int           used; /* bits used in buf */
}
MTFState_t;

static void reset(MTFState_t *s)
{
  memcpy(s->tab, DFLTTAB, TABSZ);
  s->buf  = 0;
  s->used = 0;
}

static int index_of(const MTFState_t *s, int c)
{
  int i;

  for (i = 0; i < TABSZ; i++)
    if (s->tab[i] == c)
      return i;

  return -1;
}

static void update_current(MTFState_t *s, int c)
{
  int pos;

  /* if c is in tab: push it to the front:
   * s->tab = c + [0,pos - 1] + [pos + 1,end];
   *
   * if c is not in tab: add it to the front:
   * s->tab = c + [0,length - 1]; */

  pos = index_of(s, c);
  if (pos == -1)
    pos = TABSZ - 1;

  if (pos > 0)
    memmove(s->tab + 1, s->tab, pos);
  s->tab[0] = c;
}

/* this yields a byte at a time */
static int compress(MTFState_t *s, int (*cb)(void *opaque), void *opaque)
{
  unsigned int buf;
  int          used;
  int          c;

  buf  = s->buf;
  used = s->used;

  while (used < 8)
  {
    int pos;

    /* less than a byte buffered: do a compress */

    c = cb(opaque);
    if (c == EOF)
    {
      DBUG(("compress EOF\n"));
      return EOF; // fix termination case
    }

    pos = index_of(s, c);
    if (pos != -1)
    {
      buf |= pos << used;
      used += 4;
    }
    else
    {
      buf |= (ESCAPE | (c << 4)) << used;
      used += 12;
    }
    update_current(s, c);
  }

  c = buf & 0xFF;
  buf >>= 8;
  used -= 8;

  assert(used <= 8);

  s->buf  = buf;
  s->used = used;

  DBUG(("{compress}"));

  return c;
}

/* this yields a byte at a time */
static int decompress(MTFState_t *s, int (*cb)(void *opaque), void *opaque)
{
  unsigned int buf;
  int          used;
  int          c;
  int          r;

  buf  = s->buf;
  used = s->used;

  /* ensure we have a nibble */
  if (used < 4)
  {
    DBUG(("{read 1}"));
    c = cb(opaque);
    if (c == EOF)
    {
      DBUG(("decompress EOF case 1\n"));
      return EOF; // fix termination case
    }

    DBUG(("{=%d}", c));
    buf |= c << used;
    used += 8;
  }
  else
  {
    DBUG(("{got %d bits}", used));
  }

  c = buf & 0xF;
  buf >>= 4;
  used -= 4;

  if (c != ESCAPE)
  {
    r = s->tab[c];
  }
  else
  {
    /* ensure we have a byte (duplicate ish of above) */
    if (used < 8)
    {
      DBUG(("{read 2}"));
      c = cb(opaque);
      if (c == EOF)
      {
        DBUG(("decompress EOF case 2\n"));
        return EOF; // fix termination case
      }

      DBUG(("{=%d}", c));
      buf |= c << used;
      used += 8;
    }

    r = buf & 0xFF;
    buf >>= 8;
    used -= 8;
  }

  update_current(s, r);

  s->buf  = buf;
  s->used = used;

  DBUG(("{decompress=%c}", r));

  return r;
}

/* ----------------------------------------------------------------------- */

typedef struct stream_mtfcomp
{
  stream_t      base;
  stream_t     *input;
  MTFState_t    compressor;

  int           bufsz;
  unsigned char buffer[UNKNOWN];
}
stream_mtfcomp_t;

static int comp_fetch_byte_callback(void *opaque)
{
  stream_mtfcomp_t *sm = opaque;

  return stream_getc(sm->input);
}

static int stream_mtfcomp_get(stream_t *s)
{
  stream_mtfcomp_t *sm = (stream_mtfcomp_t *) s;
  unsigned char    *p;

  assert(s->buf == s->end); /* are we only called when buffer empty? */

  /* fill the buffer up */

  for (p = sm->buffer; p < sm->buffer + sm->bufsz; )
  {
    int c;

    c = compress(&sm->compressor, comp_fetch_byte_callback, sm);
    if (c == EOF)
    {
      DBUG(("stream_mtfcomp_get hit EOF\n"));
      break;
    }

    *p++ = c;
  }

  if (p == sm->buffer)
  {
    DBUG(("no bytes generated in stream_mtfcomp_get\n"));
    return EOF; /* EOF at start */
  }

  s->buf = sm->buffer;
  s->end = p;

  return *s->buf++;
}

result_t stream_mtfcomp_create(stream_t *input, int bufsz, stream_t **s)
{
  stream_mtfcomp_t *sm;

  if (bufsz <= 0)
    bufsz = 128;

  assert(input);

  sm = malloc(offsetof(stream_mtfcomp_t, buffer) + bufsz);
  if (!sm)
    return result_OOM;

  sm->base.buf     =
    sm->base.end     = sm->buffer; /* force a fill on first use */

  sm->base.last    = result_OK;

  sm->base.op      = NULL;
  sm->base.seek    = NULL; /* can't seek */
  sm->base.get     = stream_mtfcomp_get;
  sm->base.length  = NULL; /* unknown length */
  sm->base.destroy = NULL;

  sm->input = input;
  sm->bufsz = bufsz;

  reset(&sm->compressor);

  *s = &sm->base;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

typedef struct stream_mtfdecomp
{
  stream_t      base;
  stream_t     *input;
  MTFState_t    decompressor;

  int           bufsz;
  unsigned char buffer[UNKNOWN];
}
stream_mtfdecomp_t;

static int decomp_fetch_byte_callback(void *opaque)
{
  stream_mtfdecomp_t *sm = opaque;

  return stream_getc(sm->input);
}

static int stream_mtfdecomp_get(stream_t *s)
{
  stream_mtfdecomp_t *sm = (stream_mtfdecomp_t *) s;
  unsigned char      *p;

  assert(s->buf == s->end); /* are we only called when buffer empty? */

  /* fill the buffer up */

  for (p = sm->buffer; p < sm->buffer + sm->bufsz; )
  {
    int c;

    c = decompress(&sm->decompressor, decomp_fetch_byte_callback, sm);
    if (c == EOF)
    {
      DBUG(("stream_mtfdecomp_get hit EOF\n"));
      break;
    }

    *p++ = c;
  }

  if (p == sm->buffer)
  {
    DBUG(("no bytes generated in stream_mtfdecomp_get\n"));
    return EOF; /* EOF at start */
  }

  s->buf = sm->buffer;
  s->end = p;

  return *s->buf++;
}

result_t stream_mtfdecomp_create(stream_t *input, int bufsz, stream_t **s)
{
  stream_mtfdecomp_t *sm;

  if (bufsz <= 0)
    bufsz = 128;

  assert(input);

  sm = malloc(offsetof(stream_mtfcomp_t, buffer) + bufsz);
  if (!sm)
    return result_OOM;

  sm->base.buf     =
    sm->base.end     = sm->buffer; /* force a fill on first use */

  sm->base.last    = result_OK;

  sm->base.op      = NULL;
  sm->base.seek    = NULL; /* can't seek */
  sm->base.get     = stream_mtfdecomp_get;
  sm->base.length  = NULL; /* unknown length */
  sm->base.destroy = NULL;

  sm->input = input;
  sm->bufsz = bufsz;

  reset(&sm->decompressor);

  *s = &sm->base;

  return result_OK;
}
