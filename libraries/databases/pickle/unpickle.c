/* databases/pickle/unpickle.c -- deserialise an associative array from file */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "utils/array.h"

#include "databases/pickle.h"

/* ----------------------------------------------------------------------- */

#define READBUFSZ 1024

/* ----------------------------------------------------------------------- */

typedef struct unpickle__state unpickle__state_t;

typedef result_t (*lineparser_t)(unpickle__state_t *, char *, void *);

struct unpickle__state
{
  lineparser_t                     parse;

  const pickle_writer_methods_t   *writer;
  const pickle_unformat_methods_t *unformat;

  void                            *wstate; // writer state

  char                             buffer[READBUFSZ];
};

/* ----------------------------------------------------------------------- */

static result_t unpickle__parse_line(unpickle__state_t *state,
                                     char              *buf,
                                     void              *opaque)
{
  result_t err;

  char    *key;
  char    *keyend;
  void    *fmtkey;

  char    *value;
  char    *valueend;
  void    *fmtvalue;


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

static result_t unpickle__parse_first_line(unpickle__state_t *state,
                                           char              *buf,
                                           void              *opaque)
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

result_t pickle_unpickle(const char                      *filename,
                         void                            *assocarr,
                         const pickle_writer_methods_t   *writer,
                         const pickle_unformat_methods_t *unformat,
                         void                            *opaque)
{
  const size_t       bufsz = READBUFSZ;

  result_t           err;
  FILE              *f = NULL;
  unpickle__state_t *state;
  void              *wstate = NULL;
  size_t             occupied;
  size_t             used;

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
      goto EarlyFailure; /* nothing started, so nothing to stop */
  }

  state->writer   = writer;
  state->unformat = unformat;
  state->wstate   = wstate;

  occupied = 0;
  used     = 0;

  for (;;)
  {
    size_t read;

    /* move any partial line to the start, then try to fill the buffer */

    memmove(state->buffer, state->buffer + used, occupied - used);
    occupied -= used;
    used      = 0;

    read = fread(state->buffer + occupied, 1, bufsz - occupied, f);
    if (read == 0)
    {
      if (occupied == 0)
        break; /* nothing left */

      /* the last line lacks a newline, so supply one (a partial line never
       * fills the buffer: that's a syntax error below) */
      state->buffer[occupied++] = '\n';
    }

    occupied += read;

    for (;;)
    {
      char *nl;

      nl = memchr(state->buffer + used, '\n', occupied - used);
      if (!nl)
      {
        if (used == 0 && occupied == bufsz)
        {
          /* couldn't find a \n in the whole buffer */
          err = result_PICKLE_SYNTAX_ERROR;
          goto Failure;
        }

        break; /* need more bytes */
      }

      *nl = '\0'; /* terminate */

      if (state->buffer[used] != '#') /* skip comments */
      {
        err = state->parse(state, state->buffer + used, opaque);
        if (err)
          goto Failure;
      }

      used = (nl + 1) - state->buffer; // ptrdiff_t -> int
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
