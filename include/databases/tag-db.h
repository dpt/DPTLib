/* --------------------------------------------------------------------------
 *    Name: tag-db.h
 * Purpose: Tag database
 * ----------------------------------------------------------------------- */

/**
 * \file tag-db.h
 *
 * Tag database.
 *
 * The tagdb is an associative array which maps keys (such as filenames, or
 * digests) to one or more tags. The data is stored on disc.
 *
 * PrivateEye uses this to label images.
 */

/* Use of continuations: Set the continuation value to zero to begin with,
 * it will return zero when no more tags are available. */

#ifndef DATABASES_TAG_DB_H
#define DATABASES_TAG_DB_H

#include <stddef.h>

#include "base/result.h"
#include "databases/pickle.h"

#define T tagdb

/* ----------------------------------------------------------------------- */

result_t tagdb_init(void);
void tagdb_fin(void);

/* ----------------------------------------------------------------------- */

#define tagdb_delete pickle_delete

/* ----------------------------------------------------------------------- */

typedef struct T T;

result_t tagdb_open(const char *filename, T **db);
void tagdb_close(T *db);

/* force any pending changes to disc */
result_t tagdb_commit(T *db);

/* ----------------------------------------------------------------------- */

/* tag management */

typedef unsigned int tagdb_tag;

/* add a new tag */
result_t tagdb_add(T *db, const char *name, tagdb_tag *tag);

/* delete a tag */
void tagdb_remove(T *db, tagdb_tag tag);

/* rename a tag */
result_t tagdb_rename(T *db, tagdb_tag tag, const char *name);

/* enumerate tags with counts */
result_t tagdb_enumerate_tags(T         *db,
                           int       *continuation,
                           tagdb_tag *tag,
                           int       *count);

/* convert a tag to a name */
/* 'buf' may be NULL if bufsz is 0 */
result_t tagdb_tagtoname(T         *db,
                      tagdb_tag  tag,
                      char      *buf,
                      size_t    *length,
                      size_t     bufsz);

/* ----------------------------------------------------------------------- */

/* tagging */

/* apply tag to id */
result_t tagdb_tagid(T *db, const char *id, tagdb_tag tag);

/* remove tag from id */
result_t tagdb_untagid(T *db, const char *id, tagdb_tag tag);

/* ----------------------------------------------------------------------- */

/* queries */

/* query tags for id */
result_t tagdb_get_tags_for_id(T          *db,
                            const char *id,
                            int        *continuation,
                            tagdb_tag  *tag);

/* enumerate ids */
result_t tagdb_enumerate_ids(T     *db,
                          int   *continuation,
                          char  *buf,
                          size_t bufsz);

/* enumerate ids by tag */
result_t tagdb_enumerate_ids_by_tag(T        *db,
                                 tagdb_tag tag,
                                 int      *continuation,
                                 char     *buf,
                                 size_t    bufsz);

/* enumerate ids which match all specified tags */
result_t tagdb_enumerate_ids_by_tags(T               *db,
                                  const tagdb_tag *tags,
                                  int              ntags,
                                  int             *continuation,
                                  char            *buf,
                                  size_t           bufsz);

/* ----------------------------------------------------------------------- */

/* delete knowledge of id */
void tagdb_forget(T *db, const char *id);

/* ----------------------------------------------------------------------- */

#undef T

#endif /* DATABASES_TAG_DB_H */
