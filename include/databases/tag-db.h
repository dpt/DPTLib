/* tag-db.h -- tag database */

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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "base/result.h"
#include "databases/pickle.h"

/* ----------------------------------------------------------------------- */

#define result_TAGDB_INCOMPATIBLE      (result_BASE_TAGDB + 0)
#define result_TAGDB_COULDNT_OPEN_FILE (result_BASE_TAGDB + 1)
#define result_TAGDB_SYNTAX_ERROR      (result_BASE_TAGDB + 2)
#define result_TAGDB_UNKNOWN_ID        (result_BASE_TAGDB + 3)
#define result_TAGDB_BUFF_OVERFLOW     (result_BASE_TAGDB + 4)
#define result_TAGDB_UNKNOWN_TAG       (result_BASE_TAGDB + 5)

/* ----------------------------------------------------------------------- */

#define T tagdb_t

/* ----------------------------------------------------------------------- */

result_t tagdb_init(void);
void tagdb_fin(void);

/* ----------------------------------------------------------------------- */

#define tagdb_delete pickle_delete

/* ----------------------------------------------------------------------- */

typedef struct tagdb T;

result_t tagdb_open(const char *filename, T **db);
void tagdb_close(T *db);

/* force any pending changes to disc */
result_t tagdb_commit(T *db);

/* ----------------------------------------------------------------------- */

/* tag management */

typedef unsigned int tagdb_tag_t; // make int instead?

/* add a new tag */
result_t tagdb_add(T *db, const unsigned char *name, tagdb_tag_t *tag);

/* delete a tag */
void tagdb_remove(T *db, tagdb_tag_t tag);

/* rename a tag */
result_t tagdb_rename(tagdb_t             *db,
                      tagdb_tag_t          tag,
                      const unsigned char *name);

/* enumerate tags with counts */
result_t tagdb_enumerate_tags(T           *db,
                              int         *continuation,
                              tagdb_tag_t *tag,
                              int         *count);

/* convert a tag to a name */
/* 'buf' may be NULL if bufsz is 0 */
result_t tagdb_tagtoname(tagdb_t       *db,
                         tagdb_tag_t    tag,
                         unsigned char *buf,
                         size_t        *length,
                         size_t         bufsz);

/* ----------------------------------------------------------------------- */

/* tagging */

/* apply tag to id */
result_t tagdb_tagid(T *db, const unsigned char *id, tagdb_tag_t tag);

/* remove tag from id */
result_t tagdb_untagid(T *db, const unsigned char *id, tagdb_tag_t tag);

/* ----------------------------------------------------------------------- */

/* queries */

/* query tags for id */
result_t tagdb_get_tags_for_id(T                   *db,
                               const unsigned char *id,
                               int                 *continuation,
                               tagdb_tag_t         *tag);

/* enumerate ids */
result_t tagdb_enumerate_ids(tagdb_t       *db,
                             int           *continuation,
                             unsigned char *buf,
                             size_t         bufsz);

/* enumerate ids by tag */
result_t tagdb_enumerate_ids_by_tag(tagdb_t       *db,
                                    tagdb_tag_t    tag,
                                    int           *continuation,
                                    unsigned char *buf,
                                    size_t         bufsz);

/* enumerate ids which match all specified tags */
result_t tagdb_enumerate_ids_by_tags(T                 *db,
                                     const tagdb_tag_t *tags,
                                     int                ntags,
                                     int               *continuation,
                                     unsigned char     *buf,
                                     size_t             bufsz);

/* ----------------------------------------------------------------------- */

/* delete knowledge of id */
void tagdb_forget(T *db, const unsigned char *id);

/* ----------------------------------------------------------------------- */

#undef T

#ifdef __cplusplus
}
#endif

#endif /* DATABASES_TAG_DB_H */
