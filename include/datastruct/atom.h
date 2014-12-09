/* --------------------------------------------------------------------------
 *    Name: atom.h
 * Purpose: Indexed data block storage
 * ----------------------------------------------------------------------- */

/**
 * \file atom.h
 *
 * Indexed data block store.
 *
 * Atoms are indices assigned to blocks of stored data. Identical data blocks
 * are assigned the same atom. Atoms belonging to the same set can be
 * directly compared avoiding the need to memcmp, strcmp, or otherwise
 * linearly compare the contents of the respective data blocks.
 *
 * Data blocks can be retrieved by quoting an atom to atom_get. This returns
 * a pointer to the data block along with its length.
 *
 * To avoid heap overhead, atoms are stored in a series of fixed-size pools
 * of memory. The size of the pools may be specified when the set is first
 * created.
 */

#ifndef DATASTRUCT_ATOM_H
#define DATASTRUCT_ATOM_H

#include <stddef.h>

#include "base/result.h"

/* ----------------------------------------------------------------------- */

#define result_ATOM_SET_EMPTY    (result_BASE_ATOM + 0)
#define result_ATOM_NAME_EXISTS  (result_BASE_ATOM + 1)
#define result_ATOM_OUT_OF_RANGE (result_BASE_ATOM + 2)

/* ----------------------------------------------------------------------- */

/**
 * An 'atom set' holds a collection of data blocks.
 */
typedef struct atom_set_t atom_set_t;

/* ----------------------------------------------------------------------- */

/**
 * Create a new atom set.
 *
 * This chooses default values for maximum data block size, etc.
 *
 * \return New atom set, or NULL if out of memory.
 */
atom_set_t *atom_create(void);

/**
 * Create a new atom set using the specified data sizes.
 *
 * \param locpoolsz Size of a location pool, or zero for the default.
 *                  Set this to the number of entries you typically expect to
 *                  store.
 * \param blkpoolsz Size of a block pool, or zero for the default.
 *                  No inserted data block may be larger than this.
 *                  Increasing this value will use fewer individual block
 *                  pools, reducing heap overhead, at the expense of
 *                  potentially greater wasted space should the block pool
 *                  remain not fully allocated.
 *
 * \return New atom set, or NULL if out of memory.
 */
atom_set_t *atom_create_tuned(size_t locpoolsz, size_t blkpoolsz);

/**
 * Destroy an existing atom set.
 *
 * \param doomed Atom set to destroy.
 */
void atom_destroy(atom_set_t *doomed);

/* ----------------------------------------------------------------------- */

/**
 * An atom - an index assigned to a stored block of memory.
 */
typedef int atom_t;

#define atom_NOT_FOUND ((atom_t) -1)

/* ----------------------------------------------------------------------- */

/**
 * Create a new atom from the specified data block.
 *
 * \param      set    Atom set.
 * \param      block  Data block to insert.
 * \param      length Length of data block, in bytes.
 * \param[out] atom   New atom.
 *
 * \return Error indication.
 */
result_t atom_new(atom_set_t          *set,
                  const unsigned char *block,
                  size_t               length,
                  atom_t              *atom);

/**
 * Delete an existing atom.
 *
 * \param set  Atom set.
 * \param atom Atom to delete.
 */
void atom_delete(atom_set_t *set, atom_t atom);

/**
 * Retrieve an existing data block and its length.
 *
 * \param      set    Atom set.
 * \param      atom   Atom to retrieve.
 * \param[out] length Length of data block, in bytes.
 *                    NULL if length is not required.
 *
 * \return Data block.
 */
const unsigned char *atom_get(atom_set_t *set,
                              atom_t      atom,
                              size_t     *length);

/**
 * Set an atom to hold new data.
 *
 * \param set    Atom set.
 * \param atom   Atom to set.
 * \param block  New data block.
 * \param length Length of data block, in bytes.
 *
 * \return Error indication.
 */
result_t atom_set(atom_set_t          *set,
                  atom_t               atom,
                  const unsigned char *block,
                  size_t               length);

/**
 * Retrieve the atom matching the specified data block.
 *
 * \param set    Atom set.
 * \param block  Data block to search for.
 * \param length Length of data block, in bytes.
 *
 * \return Existing atom or ATOM_NOT_FOUND.
 */
atom_t atom_for_block(atom_set_t          *set,
                      const unsigned char *block,
                      size_t               length);

/* ----------------------------------------------------------------------- */

/**
 * Delete an existing atom specified by data block.
 *
 * This is a convenience function equivalent to:
 *   atom_delete(set, atom_for_block(set, block, length)).
 *
 * \param set    Atom set.
 * \param block  Data block to retrieve.
 * \param length Length of data block, in bytes.
 */
void atom_delete_block(atom_set_t          *set,
                       const unsigned char *block,
                       size_t               length);

/* ----------------------------------------------------------------------- */

#endif /* DATASTRUCT_ATOM_H */
