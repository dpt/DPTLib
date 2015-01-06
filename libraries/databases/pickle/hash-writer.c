/* hash-writer.c -- glue methods to let pickle operate on hashes */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/suppress.h"

#include "datastruct/hash.h"

#include "databases/pickle.h"
#include "databases/pickle-writer-hash.h"

result_t pickle_writer_hash_start(void  *assocarr,
                                  void **pstate,
                                  void  *opaque)
{
  NOT_USED(opaque);

  *pstate = (void *) assocarr;

  return result_OK;
}

result_t pickle_writer_hash_next(void *state,
                                 void *key,
                                 void *value,
                                 void *opaque)
{
  NOT_USED(opaque);

  return hash_insert((hash_t *) state, key, value);
}

const pickle_writer_methods_t pickle_writer_hash =
{
  pickle_writer_hash_start,
  NULL,
  pickle_writer_hash_next
};
