/* impl.h -- atoms */

#ifndef IMPL_H
#define IMPL_H

#include <stdlib.h>

/* ----------------------------------------------------------------------- */

#define LOG2LOCPOOLSZ 5 /* default: 32 locs per pool */
#define LOG2BLKPOOLSZ 9 /* default: 512 bytes per pool */

#define LOCPTRMINSZ 4 /* minimum number of pool pointers to allocate */
#define BLKPTRMINSZ 4 /* minimum number of pool pointers to allocate */

/* ----------------------------------------------------------------------- */

/* Convert an atom into a loc. */
#define ATOMLOC(i) \
  s->locpools[(i) >> s->log2locpoolsz].locs[(i) & ((1 << s->log2locpoolsz) - 1)]
#define ATOMPTR(i) \
  (ATOMLOC(i).ptr)
#define ATOMLENGTH(i) \
  (ATOMLOC(i).length)

/* Returns true if the specified atom corresponds to an allocated (but not
 * necessarily used) index. */
#define ATOMVALID(i) \
  ((unsigned int) i < ((s->l_used - 1) << s->log2locpoolsz) + s->locpools[s->l_used - 1].used)

/* ----------------------------------------------------------------------- */

/* Stores the location and length of a block. */
typedef struct loc
{
  unsigned char *ptr;    /* pointer to block */
  int            length; /* length of block (-ve if deallocated) */
}
loc;

/* Stores the location and used count of a location pool. */
typedef struct locpool
{
  loc *locs;
  int  used;
}
locpool;

/* Stores the location and used count of a block pool. */
typedef struct blkpool
{
  unsigned char *blks;
  int            used;
}
blkpool;

struct atom_set_t
{
  size_t   log2locpoolsz; /* log2 number of locations per locpool */
  size_t   log2blkpoolsz; /* log2 number of bytes per blkpool */

  locpool *locpools;      /* growable array of location pools */
  int      l_used;
  int      l_allocated;

  blkpool *blkpools;      /* growable array of block pools */
  int      b_used;
  int      b_allocated;
};

/* ----------------------------------------------------------------------- */

result_t atom_ensure_loc_space(atom_set_t *s);
result_t atom_ensure_blk_space(atom_set_t *s, size_t length);

#endif /* IMPL_H */
