/* hash-reader.h -- glue methods to let pickle read from hashes */

#ifndef DATABASES_PICKLE_READER_HASH_H
#define DATABASES_PICKLE_READER_HASH_H

#include "databases/pickle.h"

/* Methods exposed for clients' re-use. */

pickle_reader_start_t pickle_reader_hash_start;
pickle_reader_stop_t  pickle_reader_hash_stop;
pickle_reader_next_t  pickle_reader_hash_next;

extern const pickle_reader_methods_t pickle_reader_hash;

#endif /* DATABASES_PICKLE_READER_HASH_H */
