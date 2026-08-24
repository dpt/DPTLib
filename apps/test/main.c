//
//  main.c
//  DPTLibTest
//
//  Created by David Thomas on 09/12/2014.
//  Copyright (c) 2014-2026 David Thomas. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

typedef struct test
{
  const char *name;
  testfn_t   *test;
}
test_t;

/* ----------------------------------------------------------------------- */

static const test_t tests[] =
{
  { "atom",       atom_test       },
  { "bitarr",     bitarr_test     },
  { "bitfifo",    bitfifo_test    },
  { "bitvec",     bitvec_test     },
  { "cache",      cache_test      },
  { "hash",       hash_test       },
  { "list",       list_test       },
  { "ntree",      ntree_test      },
  { "txtfmt",     txtfmt_test     },
  { "vector",     vector_test     },

  { "pickle",     pickle_test     },
  { "tagdb",      tagdb_test      },

  { "bmfont",     bmfont_test     },
  { "composite",  composite_test  },
  { "curve",      curve_test      },

  { "box",        box_test        },
  { "layout",     layout_test     },
  { "packer",     packer_test     },

  { "stream",     stream_test     },

  { "array",      array_test      },
  { "bsearch",    bsearch_test    },
  { "pack",       pack_test       },
};

static const int ntests = NELEMS(tests);

/* ----------------------------------------------------------------------- */

static int runtest(const char *resources, const test_t *t)
{
  clock_t  start, end;
  result_t rc;
  double   elapsed;

  printf(">>\n" ">> Begin %s tests\n" ">>\n", t->name);

  fprintf(stderr, "runtest trace: %s starting\n", t->name); fflush(stderr);

  start = clock();
  rc = t->test(resources);
  end = clock();

  fprintf(stderr, "runtest trace: %s finished rc=%d\n", t->name, rc); fflush(stderr);

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  if (rc != result_TEST_PASSED)
  {
    printf("** ****************\n");
    printf("** * TEST FAILING *\n");
    printf("** ****************\n");
  }

  elapsed = (double)(end - start) / CLOCKS_PER_SEC;
  printf("<<\n" "<< %s tests complete in %.4fs.\n" "<<\n" "\n", t->name, elapsed);

  return rc;
}

int main(int argc, char *argv[])
{
  const char *resources = NULL;
  int         testargvidx[100];
  int         nargs;
  int         nrun;
  int         nfailures;
  clock_t     start, end;
  int         i;
  double      elapsed;
  int         npassed;

#ifdef _MSC_VER
  /* Route assert()/runtime-check failures to stderr instead of a blocking
   * dialog box, so a CI run reports the failure instead of hanging or
   * exiting silently. */
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

  /* Line-buffer stdout so output up to the point of a crash is still
   * flushed, even when redirected (Windows fully-buffers by default). */
  setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

#ifdef FORTIFY
  Fortify_EnterScope();
#endif

  nrun      = 0;
  nfailures = 0;

  start = clock();

  nargs = 0;
  for (i = 1; i < argc; i++)
    if (strcmp(argv[i], "-resources") == 0)
      resources = argv[++i];
    else
      testargvidx[nargs++] = i;

  if (resources == NULL)
  {
    fprintf(stderr, "Error: No resources path was specified\n");
    goto cleanup;
  }

  if (nargs == 0)
  {
    /* run all tests */

    printf("++ Running all tests.\n");

    for (i = 0; i < ntests; i++)
    {
      nrun++;
      if (runtest(resources, &tests[i]) != result_TEST_PASSED)
        nfailures++;
    }
  }
  else
  {
    /* run the specified tests only */

    for (i = 0; i < nargs; i++)
    {
      int j;

      printf("++ Running specified tests only.\n");

      for (j = 0; j < ntests; j++)
        if (strcmp(tests[j].name, argv[testargvidx[i]]) == 0)
          break;

      if (j == ntests)
      {
        printf("** Unknown test '%s'.\n", argv[testargvidx[i]]);
      }
      else
      {
        nrun++;
        if (runtest(resources, &tests[j]) != result_TEST_PASSED)
          nfailures++;
      }
    }
  }

  end = clock();

  elapsed = (double)(end - start) / CLOCKS_PER_SEC;
  npassed = nrun - nfailures;
  printf("++ Tests completed in %.4fs: %d of %d tests passed.\n",
         elapsed,
         npassed,
         nrun);

cleanup:
#ifdef FORTIFY
  Fortify_LeaveScope();
  Fortify_OutputStatistics();
#endif

  exit(nfailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
