/* --------------------------------------------------------------------------
 *    Name: delete-block.c
 * Purpose: Atoms
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#include "datastruct/atom.h"

void atom_delete_block(atom_set_t          *set,
                       const unsigned char *block,
                       size_t               length)
{
  atom_delete(set, atom_for_block(set, block, length));
}
