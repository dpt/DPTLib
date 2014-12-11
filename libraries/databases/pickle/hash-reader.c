/* hash-reader.c -- glue methods to let pickle operate on hashes */

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
#include "databases/pickle-reader-hash.h"

typedef struct pickle_reader_hash_state
{
  hash_t *hash;
  int     cont;
}
pickle_reader_hash_state_t;

result_t pickle_reader_hash_start(const void *assocarr,
                                  void       *opaque,
                                  void      **pstate)
{
  pickle_reader_hash_state_t *state;

  NOT_USED(opaque);

  state = malloc(sizeof(*state));
  if (state == NULL)
    return result_OOM;

  state->hash = (hash_t *) assocarr;
  state->cont = 0;

  *pstate = state;

  return result_OK;
}

void pickle_reader_hash_stop(void *state, void *opaque)
{
  NOT_USED(opaque);

  free(state);
}

result_t pickle_reader_hash_next(void        *vstate,
                                 const void **key,
                                 const void **value,
                                 void        *opaque)
{
  result_t                    err;
  pickle_reader_hash_state_t *state = vstate;

  NOT_USED(opaque);

  err = hash_walk_continuation(state->hash,
                               state->cont,
                              &state->cont,
                               key,
                               value);

  return (err == result_HASH_END) ? result_PICKLE_END : err;
}

const pickle_reader_methods pickle_reader_hash =
{
  pickle_reader_hash_start,
  pickle_reader_hash_stop,
  pickle_reader_hash_next
};
