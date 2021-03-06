/* impl.h -- atoms */

/* An atom_set consists of a series of block pools and location pools.
 *
 * Block pools are allocated up front and filled with atoms (chunks of data)
 * until they're full, or run out of room, at which point a new block is
 * allocated.
 *
 * Location pools hold pointers to allocated blocks within the block pools.
 */

#ifndef IMPL_H
#define IMPL_H

#include <stdlib.h>

/* ----------------------------------------------------------------------- */

#define LOG2LOCPOOLSZ 5 /* default: 32 locs per pool */
#define LOG2BLKPOOLSZ 9 /* default: 512 bytes per pool */

#define LOCPTRMINSZ   4 /* minimum number of locpool_t's to allocate */
#define BLKPTRMINSZ   4 /* minimum number of blkpool_t's to allocate */

/* ----------------------------------------------------------------------- */

/**
 * Convert an atom into a loc.
 */
#define ATOMLOC(i) \
  s->locpools[(i) >> s->log2locpoolsz].locs[(i) & ((1 << s->log2locpoolsz) - 1)]

/**
 * Retrieve an atom's pointer.
 */
#define ATOMPTR(i) \
  (ATOMLOC(i).ptr)

/**
 * Retrieve an atom's length.
 */
#define ATOMLENGTH(i) \
  (ATOMLOC(i).length)

/**
 * Returns true if the specified atom corresponds to an allocated (but not
 * necessarily used) index.
 */
#define ATOMVALID(i) \
  ((unsigned int) i < ((s->l_used - 1U) << s->log2locpoolsz) + s->locpools[s->l_used - 1].used)

/* ----------------------------------------------------------------------- */

/* Stores the location and length of a block. */
typedef struct loc
{
  unsigned char  *ptr;    /* pointer into block */
  int             length; /* length of block (-ve if deallocated) */
}
loc_t;

/* Stores the location and used count of a location pool. */
typedef struct locpool
{
  struct loc     *locs;
  int             used;
}
locpool_t;

/* Stores the location and used count of a block pool. */
typedef struct blkpool
{
  unsigned char  *blks;
  int             used; // gets compared with size_t ...
}
blkpool_t;

struct atom_set
{
  size_t          log2locpoolsz; /* log2 number of locations per locpool */
  size_t          log2blkpoolsz; /* log2 number of bytes per blkpool */

  locpool_t      *locpools;      /* growable array of location pools */
  unsigned int    l_used;
  unsigned int    l_allocated;

  blkpool_t      *blkpools;      /* growable array of block pools */
  unsigned int    b_used;
  unsigned int    b_allocated;
};

/* ----------------------------------------------------------------------- */

result_t atom_ensure_loc_space(atom_set_t *s);
result_t atom_ensure_blk_space(atom_set_t *s, size_t length);

#endif /* IMPL_H */
