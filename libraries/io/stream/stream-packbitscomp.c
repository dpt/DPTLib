/* stream-packbitscomp.c -- PackBits compression */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/debug.h"
#include "base/utils.h"
#include "io/stream.h"
#include "utils/array.h"

#include "io/stream-packbits.h"

//#define DBUG(args) logf_debug args
#define DBUG(args)

/* The literal-run-literal merging can only happen within the current buffer
 * (the state is reset to Initial when resumed), so if a smaller buffer is
 * used then less merging can happen. Once the buffer is exceeded then we
 * yield and those bytes are never seen again. */

enum { Initial, Literal, Run, LiteralRun };

typedef struct stream_packbitscomp
{
  stream_t       base;
  stream_t      *input;
  int            state;
  unsigned char *lastliteral;

  int            resume; /* bool */
  int            n;
  unsigned char  first;

  int            bufsz;
  unsigned char  buffer[UNKNOWN];
}
stream_packbitscomp_t;

static result_t stream_packbitscomp_op(stream_t *s, stream_opcode_t op, void *arg)
{
  NOT_USED(s);
  NOT_USED(op);
  NOT_USED(arg);

  return result_STREAM_UNKNOWN_OP;
}

static void stream_packbitscomp_reset(stream_packbitscomp_t *c)
{
  c->state       = Initial;
  c->lastliteral = NULL;
  c->resume      = 0;
}

static stream_size_t stream_packbitscomp_fill(stream_t *s)
{
  stream_packbitscomp_t *sm = (stream_packbitscomp_t *) s;
  unsigned char         *orig_p;
  unsigned char         *p;
  unsigned char         *end;
  int                    n;
  int                    first;

  orig_p = p = (unsigned char *) sm->base.buf; // cur buf ptr
  end = sm->buffer + sm->bufsz; // abs buf end

  if (p == end)
    return stream_remaining(s); // already full

  // assert(stream_remaining(s) < need);

  if (sm->resume)
  {
    /* restore state */
    sm->state       = Initial;
    sm->lastliteral = NULL;
    sm->resume      = 0;
    n               = sm->n;
    first           = sm->first;
    goto again;
  }

  for (;;)
  {
    int second;

    /* find the longest string of identical input bytes */

    n = 1;
    first = stream_getc(sm->input);
    if (first == EOF)
      break;

    second = stream_getc(sm->input);
    while (second == first) /* terminates if EOF */
    {
      n++;
      second = stream_getc(sm->input);
    }
    /* if we didn't hit EOF, we will have read one byte too many, so put one
     * back */
    if (second != EOF)
      stream_ungetc(sm->input);

    DBUG(("stream_packbitscomp_fill: n=%d", n));

again: /* more of the current run left to pack */

    /* we assume here that we need two spare bytes to continue (which is not
     * always true) */
    if (end - p < 2)
    {
      /* save state */
      DBUG(("stream_packbitscomp_fill: %d bytes spare, saving state", end - p));
      sm->resume = 1;
      sm->n      = n;
      sm->first  = (unsigned char) first;
      break;
    }

    switch (sm->state)
    {
    case Initial: /* Initial state: Set state to 'Run' or 'Literal'. */
      if (n > 1)
      {
        DBUG(("stream_packbitscomp_fill: Initial -> Run of %d", MIN(n, 128)));
        sm->state = Run;

        /* Clamp run lengths to a maximum of 128. Technically they could go
         * up to 129, but that would generate a -128 output which is
         * specified to be ignored by the PackBits spec. */

        *p++ = (unsigned char)(-MIN(n, 128) + 1);
        *p++ = (unsigned char) first;
        n -= 128;

        if (n > 0)
          goto again;
      }
      else
      {
        DBUG(("stream_packbitscomp_fill: Initial -> Literal"));
        sm->state = Literal;

        sm->lastliteral = p;
        *p++ = 0; /* 1 repetition */
        *p++ = (unsigned char) first;
      }
      break;

    case Literal: /* Last object was a literal. */
      if (n > 1)
      {
        DBUG(("stream_packbitscomp_fill: Literal -> Run of %d", MIN(n, 128)));
        sm->state = LiteralRun;

        *p++ = (unsigned char)(-MIN(n, 128) + 1);
        *p++ = (unsigned char) first;
        n -= 128;

        if (n > 0)
          goto again;
      }
      else
      {
        DBUG(("stream_packbitscomp_fill: Literal -> Literal"));

        assert(sm->lastliteral);

        *p++ = (unsigned char) first;

        /* extend the previous literal */
        DBUG((" extending previous"));
        if (++(*sm->lastliteral) == 127)
        {
          DBUG((" -> Initial"));
          sm->state = Initial;
        }
      }
      break;

    case Run: /* Last object was a run. */
      if (n > 1)
      {
        DBUG(("stream_packbitscomp_fill: Run -> Run"));

        *p++ = (unsigned char)(-MIN(n, 128) + 1);
        *p++ = (unsigned char) first;
        n -= 128;

        if (n > 0)
          goto again;
      }
      else
      {
        DBUG(("stream_packbitscomp_fill: Run -> Literal"));
        sm->state = Literal;

        sm->lastliteral = p;
        *p++ = 0; /* 1 repetition */
        *p++ = (unsigned char) first;
      }
      break;

    case LiteralRun: /* last object was a run, preceded by a literal */
    {
      unsigned char ll;

      assert(sm->lastliteral);

      ll = *sm->lastliteral;

      /* Check to see if previous run should be converted to a literal, in
       * which case we convert literal-run-literal to a single literal. */
      if (n == 1 && p[-2] == (unsigned char) -1 && ll <= 125)
      {
        DBUG(("stream_packbitscomp_fill: LiteralRun merge literal-run-literal"));
        ll += 2; /* ..127 */
        sm->state = (ll == 127) ? Initial : Literal;
        *sm->lastliteral = ll;
        p[-2] = p[-1];
      }
      else
      {
        DBUG(("stream_packbitscomp_fill: LiteralRun -> Run"));
        sm->state = Run;
      }
      goto again;
    }
    }
  }

  if (p == orig_p)
  {
    DBUG(("stream_packbitscomp_fill: no bytes generated in decomp"));
    // EOF was returned here in older code...
  }
  else
  {
    sm->base.end = p;
  }

  return stream_remaining(s);
}

result_t stream_packbitscomp_create(stream_t *input, int bufsz, stream_t **s)
{
  stream_packbitscomp_t *sp;

  if (bufsz <= 0)
    bufsz = 128;

  if (bufsz < 2)
    return result_BAD_ARG; /* The buffer size needs to be at least 2 bytes
                            * long. */

  assert(input);

  sp = malloc(offsetof(stream_packbitscomp_t, buffer) + bufsz);
  if (!sp)
    return result_OOM;

  sp->base.buf     =
    sp->base.end     = sp->buffer; /* force a fill on first use */

  sp->base.last    = result_OK;

  sp->base.op      = stream_packbitscomp_op;
  sp->base.seek    = NULL; /* can't seek */
  sp->base.get     = stream_get;
  sp->base.fill    = stream_packbitscomp_fill;
  sp->base.length  = NULL; /* unknown length */
  sp->base.destroy = NULL;

  sp->input = input;
  sp->bufsz = bufsz;

  stream_packbitscomp_reset(sp);

  *s = &sp->base;

  return result_OK;
}
