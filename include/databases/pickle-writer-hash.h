/* --------------------------------------------------------------------------
 *    Name: hash-writer.h
 * Purpose: Glue methods to let pickle write to hashes
 * ----------------------------------------------------------------------- */

#ifndef DATABASES_PICKLE_WRITER_HASH_H
#define DATABASES_PICKLE_WRITER_HASH_H

#include "databases/pickle.h"

/* Methods exposed for clients' re-use. */

pickle_writer_start pickle_writer_hash_start;
pickle_writer_stop  pickle_writer_hash_stop;
pickle_writer_next  pickle_writer_hash_next;

extern const pickle_writer_methods pickle_writer_hash;

#endif /* DATABASES_PICKLE_WRITER_HASH_H */
