/* stream-packbitsdecomp.c -- PackBits decompression */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "io/stream.h"
#include "utils/array.h"

#include "io/stream-packbits.h"

//#define DBUG(args) logf_debug args
#define DBUG(args)

/* If we added some sort of state then we can keep the output buffer full
 * more of the time, as we'd be able to suspend and resume within the
 * repetition and literal generation steps. */

typedef struct stream_packbits_decomp
{
  stream_t      base;
  stream_t     *input;

  int           bufsz;
  unsigned char buffer[UNKNOWN]; /* The buffer size needs to be at least
                                  * that of the longest run which the
                                  * PackBits algorithm can generate, i.e. at
                                  * least 128 bytes. */
}
stream_packbits_decomp_t;

static int stream_packbits_decomp_get(stream_t *s)
{
  stream_packbits_decomp_t *sm = (stream_packbits_decomp_t *) s;
  unsigned char            *p;
  unsigned char            *end;

  /* are we only called when buffer empty? */
  assert(sm->base.buf == sm->base.end);

  p   = sm->buffer;
  end = sm->buffer + sm->bufsz;

  for (;;)
  {
    int N;

    N = stream_getc(sm->input);
    if (N == EOF)
      break;

    if (N > 128) /* repeat */
    {
      int v;

      /* N = -N + 1;         when N is signed
       * N = -(N - 256) + 1; when N is unsigned, since N > 128
       * N = -N + 257; */

      DBUG(("stream_packbits_decomp_get: decomp run: %d -> %d\n", N, -N + 257));

      N = -N + 257;

      /* ensure we have enough buffer space */

      if (p + N > end)
      {
        stream_ungetc(sm->input);
        break; /* out of buffer space */
      }

      v = stream_getc(sm->input);
      if (v == EOF)
        break; /* likely truncated data */

      memset(p, v, N);
      p += N;
    }
    else if (N < 128) /* literal */
    {
      N++; /* 0-127 on input means 1-128 bytes out */

      DBUG(("stream_packbits_decomp_get: decomp literal: %d\n", N));

      /* ensure we have enough buffer space */

      if (p + N > end)
      {
        stream_ungetc(sm->input);
        break; /* out of buffer space */
      }

      /* copy literal bytes across to output */

#if 0
      while (N--)
      {
        int v;

        v = stream_getc(sm->input);
        if (v == EOF)
          break; /* likely truncated data */

        DBUG(("{%c}\n", v));

        *p++ = v;
      }
#else
      /* more complicated version which copies directly from the source
       * stream's buffer */

      while (N)
      {
        stream_size_t avail;

        /* how much is available? */

        avail = stream_remaining_and_fill(sm->input);
        if (avail == 0)
        {
          DBUG(("*** truncated ***\n"));
          goto exit; /* likely truncated data */
        }
        avail = MIN(avail, (stream_size_t) N);

        if (avail == 1)
        {
          *p++ = *sm->input->buf++;
          N--;
        }
        else
        {
          memcpy(p, sm->input->buf, avail);
          p += avail;
          sm->input->buf += avail;
          N -= avail;
        }
      }

      assert(N == 0);
#endif
    }
    else
    {
      DBUG(("stream_packbits_decomp_get: *** 128 encountered! ***"));
      /* ignore the 128 case */
    }
  }

exit:

  if (p == sm->buffer)
  {
    DBUG(("stream_packbits_decomp_get: no bytes generated in decomp\n"));
    return EOF; /* EOF at start */
  }

  sm->base.buf = sm->buffer;
  sm->base.end = p;

  return *sm->base.buf++;
}

result_t stream_packbitsdecomp_create(stream_t *input, int bufsz, stream_t **s)
{
  stream_packbits_decomp_t *sp;

  if (bufsz <= 0)
    bufsz = 128;

  if (bufsz < 128)
    return result_BAD_ARG;

  assert(input);

  sp = malloc(offsetof(stream_packbits_decomp_t, buffer) + bufsz);
  if (!sp)
    return result_OOM;

  sp->base.buf     =
    sp->base.end     = sp->buffer; /* force a fill on first use */

  sp->base.last    = result_OK;

  sp->base.op      = NULL;
  sp->base.seek    = NULL; /* can't seek */
  sp->base.get     = stream_packbits_decomp_get;
  sp->base.length  = NULL; /* unknown length */
  sp->base.destroy = NULL;

  sp->input = input;
  sp->bufsz = bufsz;

  *s = &sp->base;

  return result_OK;
}
