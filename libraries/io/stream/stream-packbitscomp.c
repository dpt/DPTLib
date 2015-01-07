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
#include "base/suppress.h"
#include "io/stream.h"
#include "utils/array.h"
#include "utils/minmax.h"

#include "io/stream-packbits.h"

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

static int stream_packbitscomp_get(stream_t *s)
{
  stream_packbitscomp_t *sm = (stream_packbitscomp_t *) s;
  unsigned char         *p;
  unsigned char         *end;
  int                    n;
  int                    first;

  /* are we only called when buffer empty? */
  assert(sm->base.buf == sm->base.end);

  p   = sm->buffer;
  end = sm->buffer + sm->bufsz;

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

    DBUG(("stream_packbitscomp_get: n=%d\n", n));

again: /* more of the current run left to pack */

    /* we assume here that we need two spare bytes to continue (which is not
     * always true) */
    if (p + 2 > end)
    {
      /* save state */
      sm->resume = 1;
      sm->n      = n;
      sm->first  = first;
      break;
    }

    switch (sm->state)
    {
    case Initial: /* Initial state: Set state to 'Run' or 'Literal'. */
       DBUG(("stream_packbitscomp_get: Initial"));
       if (n > 1)
       {
         DBUG((" -> Run of %d\n", MIN(n, 128)));
         sm->state = Run;

         /* Clamp run lengths to a maximum of 128. Technically they could go
          * up to 129, but that would generate a -128 output which is
          * specified to be ignored by the PackBits spec. */

         *p++ = -MIN(n, 128) + 1;
         *p++ = first;
         n -= 128;

         if (n > 0)
           goto again;
       }
       else
       {
         DBUG((" -> Literal\n"));
         sm->state = Literal;

         sm->lastliteral = p;
         *p++ = 0; /* 1 repetition */
         *p++ = first;
       }
       break;

     case Literal: /* Last object was a literal. */
       DBUG(("stream_packbitscomp_get: Literal"));
       if (n > 1)
       {
         DBUG((" -> Run of %d\n", MIN(n, 128)));
         sm->state = LiteralRun;

         *p++ = -MIN(n, 128) + 1;
         *p++ = first;
         n -= 128;

         if (n > 0)
           goto again;
       }
       else
       {
         DBUG((" -> Literal\n"));

         assert(sm->lastliteral);

         *p++ = first;

         /* extend the previous literal */
         DBUG((" extending previous\n"));
         if (++(*sm->lastliteral) == 127)
         {
           DBUG((" -> Initial\n"));
           sm->state = Initial;
         }
       }
       break;

     case Run: /* Last object was a run. */
       DBUG(("stream_packbitscomp_get: Run"));
       if (n > 1)
       {
         DBUG((" -> Run\n"));

         *p++ = -MIN(n, 128) + 1;
         *p++ = first;
         n -= 128;

         if (n > 0)
             goto again;
       }
       else
       {
         DBUG((" -> Literal\n"));
         sm->state = Literal;

         sm->lastliteral = p;
         *p++ = 0; /* 1 repetition */
         *p++ = first;
       }
       break;

     case LiteralRun: /* last object was a run, preceded by a literal */
       {
       int ll;

       DBUG(("stream_packbitscomp_get: LiteralRun"));

       assert(sm->lastliteral);

       ll = *sm->lastliteral;

       /* Check to see if previous run should be converted to a literal, in
        * which case we convert literal-run-literal to a single literal. */
       if (n == 1 && p[-2] == (unsigned char) -1 && ll < 126)
       {
           DBUG((" merge literal-run-literal\n"));
           ll += 2;
           sm->state = (ll == 127) ? Initial : Literal;
           *sm->lastliteral = ll;
           p[-2] = p[-1];
       }
       else
       {
           DBUG((" -> Run\n"));
           sm->state = Run;
       }
       goto again;

       }
    }
  }

  if (p == sm->buffer)
  {
    DBUG(("stream_packbitscomp_get: no bytes generated in decomp\n"));
    return EOF; /* EOF at start */
  }

  sm->base.buf = sm->buffer;
  sm->base.end = p;

  return *sm->base.buf++;
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
  sp->base.get     = stream_packbitscomp_get;
  sp->base.length  = NULL; /* unknown length */
  sp->base.destroy = NULL;

  sp->input = input;
  sp->bufsz = bufsz;

  stream_packbitscomp_reset(sp);

  *s = &sp->base;

  return result_OK;
}
