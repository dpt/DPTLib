/* --------------------------------------------------------------------------
 *    Name: hash-reader.h
 * Purpose: Glue methods to let pickle read from hashes
 * ----------------------------------------------------------------------- */

#ifndef DATABASES_PICKLE_READER_HASH_H
#define DATABASES_PICKLE_READER_HASH_H

#include "databases/pickle.h"

/* Methods exposed for clients' re-use. */

pickle_reader_start pickle_reader_hash_start;
pickle_reader_stop  pickle_reader_hash_stop;
pickle_reader_next  pickle_reader_hash_next;

extern const pickle_reader_methods pickle_reader_hash;

#endif /* DATABASES_PICKLE_READER_HASH_H */
