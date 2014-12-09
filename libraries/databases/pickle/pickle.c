/* pickle.c */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "fortify/fortify.h"

#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osgbpb.h"

#include "oslib/types.h"

#include "appengine/types.h"
#include "appengine/base/errors.h"

#include "appengine/databases/pickle.h"

/* ----------------------------------------------------------------------- */

static const char signature[] = PICKLE_SIGNATURE;

/* ----------------------------------------------------------------------- */

/* write out a version numbered header */
static result_t pickle__write_header(os_fw f, const char *comments, size_t len)
{
  // FIXME: Could use printf style here, e.g.:
  //        "%c %s\n%s\n", commentchar, comments, signature

  static const char commentchar[] = "# ";

  static const struct
  {
    const char *line;
    size_t      length;
  }
  lines[] =
  {
    { signature, sizeof(signature) - 1 },
  };

  os_error *oserr;
  int       i;

  oserr = xosgbpb_writew(f,
          (const byte *) commentchar,
                         NELEMS(commentchar) - 1,
                         NULL);
  if (oserr)
    goto oserrexit;

  oserr = xosgbpb_writew(f,
          (const byte *) comments,
                         len,
                         NULL);
  if (oserr)
    goto oserrexit;

  oserr = xos_bput('\n', f);
  if (oserr)
    goto oserrexit;

  for (i = 0; i < NELEMS(lines); i++)
  {
    oserr = xosgbpb_writew(f,
            (const byte *) lines[i].line,
                           lines[i].length,
                           NULL);
    if (oserr)
      goto oserrexit;

    oserr = xos_bput('\n', f);
    if (oserr)
      goto oserrexit;
  }

  return result_OK;


oserrexit:

  return result_OS;
}

static result_t pickle__write_body(os_fw                        f,
                                   void                        *assocarr,
                                   const pickle_reader_methods *reader,
                                   const pickle_format_methods *format,
                                   void                        *opaque)
{
  result_t       err;
  os_error   *oserr;
  void       *state;
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


    oserr = xosgbpb_writew(f, (byte *) buffer1, strlen(buffer1), NULL);
    if (oserr)
      goto oserrexit;

    oserr = xosgbpb_writew(f,
            (const byte *) format->split,
                           format->splitlen,
                           NULL);
    if (oserr)
      goto oserrexit;

    oserr = xosgbpb_writew(f, (byte *) buffer2, strlen(buffer2), NULL);
    if (oserr)
      goto oserrexit;

    oserr = xos_bput('\n', f);
    if (oserr)
      goto oserrexit;
  }

  if (err == result_PICKLE_END)
    err = result_OK;

exit:

  if (reader->stop)
    reader->stop(state, opaque);

  return err;


oserrexit:

  err = result_OS;
  goto exit;
}

/* ----------------------------------------------------------------------- */

result_t pickle_pickle(const char                  *filename,
                       void                        *assocarr,
                       const pickle_reader_methods *reader,
                       const pickle_format_methods *format,
                       void                        *opaque)
{
  result_t err;
  os_fw f;

  assert(filename);
  assert(assocarr);
  assert(reader);
  assert(format);

  assert(reader->next); /* the other two methods can be NULL */

  f = osfind_openoutw(osfind_NO_PATH, filename, NULL);
  if (f == 0)
    return result_FILE_OPEN_FAILED;

  err = pickle__write_header(f, format->comments, format->commentslen);
  if (err)
    goto failure;

  err = pickle__write_body(f, assocarr, reader, format, opaque);
  if (err)
    goto failure;

  osfind_close(f);

  xosfile_set_type(filename, osfile_TYPE_TEXT);

  return result_OK;


failure:

  osfind_close(f);

  xosfile_set_type(filename, osfile_TYPE_DATA);

  return err;
}
