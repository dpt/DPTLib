/* hash-writer.h -- glue methods to let pickle write to hashes */

#ifndef DATABASES_PICKLE_WRITER_HASH_H
#define DATABASES_PICKLE_WRITER_HASH_H

#include "databases/pickle.h"

/* Methods exposed for clients' re-use. */

pickle_writer_start_t pickle_writer_hash_start;
pickle_writer_stop_t  pickle_writer_hash_stop;
pickle_writer_next_t  pickle_writer_hash_next;

extern const pickle_writer_methods_t pickle_writer_hash;

#endif /* DATABASES_PICKLE_WRITER_HASH_H */
