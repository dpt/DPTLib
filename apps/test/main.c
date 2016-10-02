//
//  main.c
//  DPTLibTest
//
//  Created by David Thomas on 09/12/2014.
//  Copyright (c) 2014 David Thomas. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

/* ----------------------------------------------------------------------- */

typedef result_t (testfn_t)(void);

typedef struct test
{
  const char *name;
  testfn_t   *test;
}
test_t;

/* ----------------------------------------------------------------------- */

/* datastruct */
extern testfn_t atom_test,
                cache_test,
                bitarr_test,
                bitvec_test,
                hash_test,
                list_test,
                ntree_test,
                vector_test;

/* database */
extern testfn_t pickle_test,
                tagdb_test;

/* geom */
extern testfn_t layout_test,
                packer_test;

/* io */
extern testfn_t stream_test;

/* utils */
extern testfn_t array_test,
                bsearch_test;

/* ----------------------------------------------------------------------- */

static const test_t tests[] =
{
  { "atom",     atom_test     },
  { "bitarr",   bitarr_test   },
  { "bitvec",   bitvec_test   },
  { "cache",    cache_test    },
  { "hash",     hash_test     },
  { "list",     list_test     },
  { "ntree",    ntree_test    },
  { "vector",   vector_test   },

  { "pickle",   pickle_test   },
  { "tagdb",    tagdb_test    },

  { "layout",   layout_test   },
  { "packer",   packer_test   },

  { "stream",   stream_test   },

  { "array",    array_test    },
  { "bsearch",  bsearch_test  },
};

static const int ntests = NELEMS(tests);

/* ----------------------------------------------------------------------- */

static int runtest(const test_t *t)
{
  result_t rc;

  printf(">>\n" ">> Begin %s tests\n" ">>\n", t->name);

  rc = t->test();

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  if (rc != result_TEST_PASSED)
  {
    printf("** ****************\n");
    printf("** * TEST FAILING *\n");
    printf("** ****************\n");
  }

  printf("<<\n" "<< %s tests complete.\n" "<<\n" "\n", t->name);

  return rc;
}

int main(int argc, char *argv[])
{
  int nrun;
  int nfailures;
  int i;
  int npassed;

#ifdef FORTIFY
  Fortify_EnterScope();
#endif

  nrun      = 0;
  nfailures = 0;

  if (argc < 2)
  {
    /* run all tests */

    printf("++ Running all tests.\n");

    for (i = 0; i < ntests; i++)
    {
      nrun++;
      if (runtest(&tests[i]) != result_TEST_PASSED)
        nfailures++;
    }
  }
  else
  {
    /* run the specified tests only */

    for (i = 1; i < argc; i++)
    {
      int j;

      printf("++ Running specified tests only.\n");

      for (j = 0; j < ntests; j++)
        if (strcmp(tests[j].name, argv[i]) == 0)
          break;

      if (j == ntests)
      {
        printf("** Unknown test '%s'.\n", argv[i]);
      }
      else
      {
        nrun++;
        if (runtest(&tests[j]) != result_TEST_PASSED)
          nfailures++;
      }
    }
  }

  npassed = nrun - nfailures;
  printf("++ Tests completed: %d of %d tests passed.\n", npassed, nrun);

#ifdef FORTIFY
  Fortify_LeaveScope();
  Fortify_OutputStatistics();
#endif

  exit(nfailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
