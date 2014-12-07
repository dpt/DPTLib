/* --------------------------------------------------------------------------
 *    Name: filename-db.c
 * Purpose: Filename database
 * ----------------------------------------------------------------------- */

/* filenamedb maps md5 digests to filenames so that we can search for files
 * and retrieve filenames */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#ifdef __riscos
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#endif

#include "base/suppress.h"
#include "base/result.h"
//#include "base/strings.h"
#include "databases/digest-db.h"
#include "databases/pickle-reader-hash.h"
#include "databases/pickle-writer-hash.h"
#include "databases/pickle.h"
#include "utils/array.h"
#include "datastruct/atom.h"
#include "datastruct/hash.h"

#include "databases/filename-db.h"

/* ----------------------------------------------------------------------- */

/* Hash bins. */
#define HASHSIZE 97

/* Long enough to hold any database line. */
#define READBUFSZ 1024

/* Size of pools used for atom storage. */
#define ATOMBUFSZ 32768

/* Estimated filename length. */
#define ESTATOMLEN 80

/* ----------------------------------------------------------------------- */

static unsigned int filenamedb_refcount = 0;

result_t filenamedb_init(void)
{
  result_t err;

  if (filenamedb_refcount == 0)
  {
    /* dependencies */

    err = digestdb_init();
    if (err)
      return err;
  }

  filenamedb_refcount++;

  return result_OK;
}

void filenamedb_fin(void)
{
  if (filenamedb_refcount == 0)
    return;

  if (--filenamedb_refcount == 0)
  {
    /* dependencies */

    digestdb_fin();
  }
}

/* ----------------------------------------------------------------------- */

struct filenamedb_t
{
  char       *filename;

  atom_set_t *filenames;
  hash_t     *hash;
};

/* ----------------------------------------------------------------------- */

/* This is identical to tagdb.c's unformat_key... */
static result_t unformat_key(const char *buf,
                          size_t      len,
                          void      **key,
                          void       *opaque)
{
  result_t         err;
  unsigned char hash[digestdb_DIGESTSZ];
  int           kindex;

  NOT_USED(len);
  NOT_USED(opaque);

  // if (len != 32) then complain

  /* convert ID from ASCII hex to binary */
  err = digestdb_decode(hash, buf);
  if (err)
    return err;

  err = digestdb_add(hash, &kindex);
  if (err)
    return err;

  *key = (void *) digestdb_get(kindex); /* must cast away const */

  return result_OK;
}

static result_t unformat_value(const char *buf,
                            size_t      len,
                            void      **value,
                            void       *opaque)
{
  result_t         err;
  filenamedb_t *db = opaque;
  atom_t        vindex;

  NOT_USED(len);

  err = atom_new(db->filenames,
                 (const unsigned char *) buf,
                 strlen(buf) + 1, // use 'len'?
                &vindex);
  if (err == result_ATOM_NAME_EXISTS)
    err = result_OK;
  else if (err)
    return err;

  *value = (void *) atom_get(db->filenames, vindex, NULL); // casting away const

  return result_OK;
}

static const pickle_unformat_methods unformat_methods =
{
  " ", /* split string */
  1,   /* split string length */
  unformat_key,
  unformat_value
};

/* ----------------------------------------------------------------------- */

result_t filenamedb_open(const char *filename, filenamedb_t **pdb)
{
  result_t         err;
  char         *filenamecopy = NULL;
  atom_set_t   *filenames    = NULL;
  hash_t       *hash         = NULL;
  filenamedb_t *db           = NULL;

  assert(filename);
  assert(pdb);

  filenamecopy = strdup(filename); // FIXME was str_dup
  if (filenamecopy == NULL)
  {
    err = result_OOM;
    goto Failure;
  }

  filenames = atom_create_tuned(ATOMBUFSZ / ESTATOMLEN, ATOMBUFSZ);
  if (filenames == NULL)
  {
    err = result_OOM;
    goto Failure;
  }

  err = hash_create(HASHSIZE,
                    digestdb_hash,
                    digestdb_compare,
                    hash_no_destroy_key,
                    hash_no_destroy_value,
                   &hash);
  if (err)
    goto Failure;

  db = malloc(sizeof(*db));
  if (db == NULL)
  {
    err = result_OOM;
    goto Failure;
  }

  db->filename    = filenamecopy;
  db->filenames   = filenames;
  db->hash        = hash;

  /* read the database in */
  err = pickle_unpickle(filename,
                        db->hash,
                       &pickle_writer_hash,
                       &unformat_methods,
                        db);
  if (err && err != result_PICKLE_COULDNT_OPEN_FILE)
    goto Failure;

  *pdb = db;

  return result_OK;


Failure:

  free(db);
  hash_destroy(hash);
  atom_destroy(filenames);
  free(filenamecopy);

  return err;
}

void filenamedb_close(filenamedb_t *db)
{
  if (db == NULL)
    return;

  filenamedb_commit(db);

  hash_destroy(db->hash);
  atom_destroy(db->filenames);

  free(db->filename);

  free(db);
}

/* ----------------------------------------------------------------------- */

static result_t format_key(const void *vkey,
                        char       *buf,
                        size_t      len,
                        void       *opaque)
{
  NOT_USED(opaque);

  if (len < digestdb_DIGESTSZ * 2 + 1)
    return result_FILENAMEDB_BUFF_OVERFLOW;

  digestdb_encode(buf, vkey);
  buf[digestdb_DIGESTSZ * 2] = '\0';

  return result_OK;
}

static result_t format_value(const void *vvalue,
                          char       *buf,
                          size_t      len,
                          void       *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  strcpy(buf, vvalue);

  return result_OK;
}

static const pickle_format_methods format_methods =
{
  "Filenames",
  NELEMS("Filenames") - 1,
  " ",
  1,
  format_key,
  format_value
};

/* ----------------------------------------------------------------------- */

result_t filenamedb_commit(filenamedb_t *db)
{
  result_t err;

  err = pickle_pickle(db->filename,
                      db->hash,
                     &pickle_reader_hash,
                     &format_methods,
                      db);
  if (err)
    return err;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t filenamedb_add(filenamedb_t *db,
                     const char   *id,
                     const char   *filename)
{
  result_t                err;
  int                  kindex;
  atom_t               vindex;
  const unsigned char *key;
  const char          *value;

  err = digestdb_add((const unsigned char *) id, &kindex);
  if (err)
    return err;

  key = digestdb_get(kindex);

  err = atom_new(db->filenames, (const unsigned char *) filename,
                 strlen(filename) + 1, &vindex);
  if (err == result_ATOM_NAME_EXISTS)
    err = result_OK;
  else if (err)
    return err;

  value = (const char *) atom_get(db->filenames, vindex, NULL);

  /* this will update the value if the key is already present */

  hash_insert(db->hash, (unsigned char *) key, (char *) value);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

const char *filenamedb_get(filenamedb_t *db,
                           const char   *id)
{
  return hash_lookup(db->hash, id);
}

/* ----------------------------------------------------------------------- */

static int prune_cb(const void *key, const void *value, void *opaque)
{
  filenamedb_t           *db = opaque;
#ifdef __riscos
  fileswitch_object_type  object_type;

  /* does the file exist? */
  object_type = osfile_read_no_path(value, NULL, NULL, NULL, NULL);
  if (object_type == fileswitch_NOT_FOUND)
  {
    /* if not, delete it */
    hash_remove(db->hash, key);
  }
#endif
  
  return 0;
}

result_t filenamedb_prune(filenamedb_t *db)
{
  hash_walk(db->hash, prune_cb, db);

  return result_OK;
}
