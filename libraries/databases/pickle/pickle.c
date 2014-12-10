/* pickle.c */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "utils/array.h"

#include "databases/pickle.h"

/* ----------------------------------------------------------------------- */

static const char signature[] = PICKLE_SIGNATURE;

/* ----------------------------------------------------------------------- */

/* write out a version numbered header */
static result_t pickle__write_header(FILE       *f,
                                     const char *comments,
                                     size_t      commentslen)
{
  static const char commentchar[] = "# ";
  int rc;

  rc = fprintf(f, "%s%.*s\n%s\n",
               commentchar,
               (int) commentslen,
               comments,
               signature);
  if (rc < 0)
    return result_BUFFER_OVERFLOW; // FIXME hard coded error number (could use errno?)

  return result_OK;
}

static result_t pickle__write_body(FILE                        *f,
                                   void                        *assocarr,
                                   const pickle_reader_methods *reader,
                                   const pickle_format_methods *format,
                                   void                        *opaque)
{
  result_t    err;
  void       *state = NULL;
  const void *key;
  const void *value;

  if (reader->start)
  {
    err = reader->start(assocarr, opaque, &state);
    if (err)
      return err;
  }

  while ((err = reader->next(state, &key, &value, opaque)) == result_OK)
  {
    char buffer1[256];
    char buffer2[768];
    int  rc;

    // FIXME: get the format methods to pass back how much they used then we
    //        can pack the entire lot into the output buffer as we go

    err = format->key(key, buffer1, NELEMS(buffer1), opaque);
    if (err == result_PICKLE_SKIP)
      continue;
    else if (err)
      goto exit;

    err = format->value(value, buffer2, NELEMS(buffer2), opaque);
    if (err == result_PICKLE_SKIP)
      continue;
    else if (err)
      goto exit;

    rc = fprintf(f, "%s%.*s%s\n",
                 buffer1,
                 (int) format->splitlen, format->split,
                 buffer2);
    if (rc < 0)
    {
      err = result_BUFFER_OVERFLOW; // FIXME hard coded error number (could use errno?)
      goto exit;
    }
  }

  if (err == result_PICKLE_END)
    err = result_OK;

exit:

  if (reader->stop)
    reader->stop(state, opaque);

  return err;
}

/* ----------------------------------------------------------------------- */

result_t pickle_pickle(const char                  *filename,
                       void                        *assocarr,
                       const pickle_reader_methods *reader,
                       const pickle_format_methods *format,
                       void                        *opaque)
{
  result_t err;
  FILE    *f;

  assert(filename);
  assert(assocarr);
  assert(reader);
  assert(format);

  assert(reader->next); /* the other two methods can be NULL */

  f = fopen(filename, "wb");
  if (f == NULL)
    return result_PICKLE_COULDNT_OPEN_FILE;

  err = pickle__write_header(f, format->comments, format->commentslen);
  if (err)
    goto failure;

  err = pickle__write_body(f, assocarr, reader, format, opaque);
  if (err)
    goto failure;

  fclose(f);

  return result_OK;


failure:

  fclose(f);

  return err;
}
