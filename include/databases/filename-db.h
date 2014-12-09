/* filename-db.h -- filename database */

/**
 * \file filename-db.h
 *
 * Filename database.
 *
 * The filenamedb is an associative array which maps keys (such as digests)
 * to filenames. The data is stored on disc.
 *
 * PrivateEye uses this to find out where an image lives given its digest.
 */

#ifndef DATABASES_FILENAME_DB_H
#define DATABASES_FILENAME_DB_H

#include <stddef.h>

#include "base/result.h"
#include "databases/pickle.h"

/* ----------------------------------------------------------------------- */

#define result_FILENAMEDB_INCOMPATIBLE      (result_BASE_FILENAMEDB + 0)
#define result_FILENAMEDB_COULDNT_OPEN_FILE (result_BASE_FILENAMEDB + 1)
#define result_FILENAMEDB_SYNTAX_ERROR      (result_BASE_FILENAMEDB + 2)
#define result_FILENAMEDB_BUFF_OVERFLOW     (result_BASE_FILENAMEDB + 3)

/* ----------------------------------------------------------------------- */

#define T filenamedb_t

/* ----------------------------------------------------------------------- */

result_t filenamedb_init(void);
void filenamedb_fin(void);

/* ----------------------------------------------------------------------- */

#define filenamedb_delete pickle_delete

/* ----------------------------------------------------------------------- */

typedef struct T T;

result_t filenamedb_open(const char *filename, T **db);
void filenamedb_close(T *db);

/* force any pending changes to disc */
result_t filenamedb_commit(T *db);

/* ----------------------------------------------------------------------- */

result_t filenamedb_add(T          *db,
                        const char *id,
                        const char *filename);

const char *filenamedb_get(T          *db,
                           const char *id);

/* ----------------------------------------------------------------------- */

/* delete knowledge of filenames which don't exist on disc */
result_t filenamedb_prune(T *db);

/* ----------------------------------------------------------------------- */

#undef T

#endif /* DATABASES_FILENAME_DB_H */
