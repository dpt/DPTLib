/* --------------------------------------------------------------------------
 *    Name: digest-db.h
 * Purpose: Digest database
 * ----------------------------------------------------------------------- */

/**
 * \file digest-db.h
 *
 * Digest database.
 *
 * The digestdb is a static in-memory array dedicated to holding (128-bit)
 * digests.
 *
 * digestdb is used by tagdb and filenamedb to save memory by sharing the
 * digest storage cost.
 */

#ifndef DATABASES_DIGEST_DB_H
#define DATABASES_DIGEST_DB_H

#include <stddef.h>

#include "base/result.h"

/* ----------------------------------------------------------------------- */

#define digestdb_DIGESTSZ 16

/* ----------------------------------------------------------------------- */

result_t digestdb_init(void);
void digestdb_fin(void);

/* ----------------------------------------------------------------------- */

result_t digestdb_add(const unsigned char *digest, int *index);
const unsigned char *digestdb_get(int index);

/* ----------------------------------------------------------------------- */

unsigned int digestdb_hash(const void *a);
int digestdb_compare(const void *a, const void *b);

/* ----------------------------------------------------------------------- */

/* Decode 32 characters of ASCII hex to 16 bytes.
 * Returns result_BAD_ARG if nonhex data is encountered. */
result_t digestdb_decode(unsigned char *digest, const char *text);

/* Encode 16 bytes to 32 characters of ASCII hex. */
void digestdb_encode(char *text, const unsigned char *digest);

/* ----------------------------------------------------------------------- */

#endif /* DATABASES_DIGEST_DB_H */