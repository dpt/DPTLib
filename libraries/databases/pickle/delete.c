/* delete.c */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __riscos
#include "oslib/osfile.h"
#endif

#include "databases/pickle.h"

void pickle_delete(const char *filename)
{
  assert(filename);

#ifdef __riscos
  xosfile_delete(filename, NULL, NULL, NULL, NULL, NULL);
#else
  remove(filename);
#endif
}
