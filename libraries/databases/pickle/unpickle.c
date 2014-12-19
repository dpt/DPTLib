/* unpickle.c -- deserialise an associative array from file */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/suppress.h"
#include "utils/array.h"

#include "databases/pickle.h"

/* ----------------------------------------------------------------------- */

#define READBUFSZ 1024

/* ----------------------------------------------------------------------- */

typedef struct unpickle__state unpickle__state;

typedef result_t (*lineparser)(unpickle__state *, char *, void *);

struct unpickle__state
{
  lineparser                     parse;

  const pickle_writer_methods   *writer;
  const pickle_unformat_methods *unformat;

  void                          *wstate; // writer state

  char                           buffer[READBUFSZ];
};

/* ----------------------------------------------------------------------- */

static result_t unpickle__parse_line(unpickle__state *state,
                                     char            *buf,
                                     void            *opaque)
{
  result_t err;

  char *key;
  char *keyend;
  void *fmtkey;

  char *value;
  char *valueend;
  void *fmtvalue;


  key = buf;

  // this requires the split str to be terminated...
  keyend = strstr(key, state->unformat->split); /* split at space */
  if (keyend == NULL)
    return result_PICKLE_SYNTAX_ERROR; /* end of string */

  *keyend++ = '\0'; /* terminate previous token */


  value = keyend - 1 + state->unformat->splitlen;

//  /* skip any further split chars */
//  while (*value == SPLIT)
//    value++;

  if (*value == '\0')
    return result_PICKLE_SYNTAX_ERROR; /* end of string */

  valueend = value + strlen(value) + 1;


  /* return lengths inclusive of terminator */

  err = state->unformat->key(key, keyend - key, &fmtkey, opaque);
  if (err)
    return err;

  err = state->unformat->value(value, valueend - value, &fmtvalue, opaque);
  if (err)
    return err; // no result_t cleanup


  err = state->writer->next(state->wstate, fmtkey, fmtvalue, opaque);
  if (err)
    return err; // no result_t cleanup


  return result_OK;
}

static result_t unpickle__parse_first_line(unpickle__state *state,
                                           char            *buf,
                                           void            *opaque)
{
  static const char signature[] = PICKLE_SIGNATURE;

  NOT_USED(opaque);

  /* validate the db signature */

  if (strcmp(buf, signature) != 0)
    return result_PICKLE_INCOMPATIBLE;

  state->parse = unpickle__parse_line;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t pickle_unpickle(const char                    *filename,
                         void                          *assocarr,
                         const pickle_writer_methods   *writer,
                         const pickle_unformat_methods *unformat,
                         void                          *opaque)
{
  result_t         err;
  const size_t     bufsz = READBUFSZ;
  FILE            *f = NULL;
  unpickle__state *state;
  int              occupied;
  int              used;
  void            *wstate = NULL;

  f = fopen(filename, "rb");
  if (f == NULL)
    return result_PICKLE_COULDNT_OPEN_FILE;

  state = malloc(sizeof(*state));
  if (state == NULL)
  {
    err = result_OOM;
    goto EarlyFailure;
  }

  state->parse = unpickle__parse_first_line;

  if (writer->start)
  {
    err = writer->start(assocarr, &wstate, opaque);
    if (err)
      goto Failure;
  }

  state->writer   = writer;
  state->unformat = unformat;
  state->wstate   = wstate;

  occupied = 0;
  used     = 0;

  for (;;)
  {
    size_t read;

    /* try to fill buffer */

    read = fread(state->buffer + occupied, 1, bufsz - occupied, f);
    if (read == 0 && feof(f))
      break; /* nothing left */

    occupied += read;

    for (;;)
    {
      char *nl;

      nl = memchr(state->buffer + used, '\n', occupied - used);
      if (!nl)
      {
        if (used == 0)
        {
          /* couldn't find a \n in the whole buffer - and used is 0, so we
           * have the whole buffer in which to look */
          err = result_PICKLE_SYNTAX_ERROR;
          goto Failure;
        }
        else
        {
          /* make space in the buffer, then get it refilled */

          memmove(state->buffer, state->buffer + used, occupied - used);
          occupied -= used;
          used      = 0;

          break; /* need more bytes */
        }
      }

      *nl = '\0'; /* terminate */

      if (state->buffer[used] != '#') /* skip comments */
      {
        err = state->parse(state, state->buffer + used, opaque);
        if (err)
          goto Failure;
      }

      used = (nl + 1) - state->buffer;
      if (occupied - used <= 0)
        break;
    }
  }

  err = result_OK;

  /* FALLTHROUGH */

Failure:

  if (writer->stop)
    writer->stop(wstate, opaque);

EarlyFailure:

  fclose(f);

  free(state);

  return err;
}
