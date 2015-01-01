/* digest-db.c -- digest database */

#include <stddef.h>
#include <string.h>

#include "base/result.h"

#include "datastruct/atom.h"

#include "databases/digest-db.h"

/* ----------------------------------------------------------------------- */

/* Size of pools used for atom storage. */
#define ATOMBUFSZ 32768

/* ----------------------------------------------------------------------- */


static struct
{
  /* We use an atom set as the store at the moment. This is a bit wasteful as
   * a length field is stored per entry and it's always 16 bytes. */
  atom_set_t *digests;
}
LOCALS;

/* ----------------------------------------------------------------------- */

static unsigned int digestdb_refcount = 0;

result_t digestdb_init(void)
{
  if (digestdb_refcount == 0)
  {
    /* initialise */

    LOCALS.digests = atom_create_tuned(ATOMBUFSZ / digestdb_DIGESTSZ,
                                       ATOMBUFSZ);
    if (LOCALS.digests == NULL)
      return result_OOM;
  }

  digestdb_refcount++;

  return result_OK;
}

void digestdb_fin(void)
{
  if (digestdb_refcount == 0)
    return;

  if (--digestdb_refcount == 0)
  {
    /* finalise */

    atom_destroy(LOCALS.digests);
  }
}

/* ----------------------------------------------------------------------- */

result_t digestdb_add(const unsigned char *digest, int *index)
{
  result_t err;

  err = atom_new(LOCALS.digests, digest, digestdb_DIGESTSZ, (atom_t *) index);
  if (err == result_ATOM_NAME_EXISTS)
    err = result_OK;

  return err;
}

const unsigned char *digestdb_get(int index)
{
  return atom_get(LOCALS.digests, index, NULL);
}

/* ----------------------------------------------------------------------- */

unsigned int digestdb_hash(const void *a)
{
  const unsigned char *ma = a;

  /* it's already a hash: just mask off enough bits */

  return *ma; /* assumes <= 256 hash bins */
}

int digestdb_compare(const void *a, const void *b)
{
  const unsigned char *ma = a;
  const unsigned char *mb = b;

  return memcmp(ma, mb, digestdb_DIGESTSZ);
}

/* ----------------------------------------------------------------------- */

result_t digestdb_decode(unsigned char *bytes, const char *text)
{
#define _ 255

  static const unsigned char tab[] =
  {
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, _, _, _, _, _, _,
    _,10,11,12,13,14,15, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _,10,11,12,13,14,15, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
  };

  const char *end;
  int         lo, hi;

  end = text + digestdb_DIGESTSZ * 2;
  for (; text < end; text += 2)
  {
    hi = tab[text[0]];
    lo = tab[text[1]];

    if (lo == _ || hi == _)
      return result_BAD_ARG;

    *bytes++ = (hi << 4) | lo;
  }

#undef _

  return result_OK;
}

void digestdb_encode(char *text, const unsigned char *bytes)
{
  static const char tab[] = "0123456789abcdef";

  const unsigned char *end;

  end = bytes + digestdb_DIGESTSZ;
  for (; bytes < end; bytes++)
  {
    unsigned char b;

    b = *bytes;

    *text++ = tab[(b & 0xf0) >> 4];
    *text++ = tab[(b & 0x0f) >> 0];
  }
}

/* ----------------------------------------------------------------------- */

