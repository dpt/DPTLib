//
//  main.c
//  DPTLibTest
//
//  Created by David Thomas on 09/12/2014.
//  Copyright (c) 2014 David Thomas. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"

/* datastruct */
extern result_t array_test(void);
extern result_t atom_test(void);
extern result_t bitarr_test(void);
extern result_t bitvec_test(void);
extern result_t hash_test(void);
extern result_t list_test(void);
extern result_t ntree_test(void);
extern result_t vector_test(void);

/* database */
extern result_t pickle_test(void);
extern result_t tagdb_test(void);

/* geom */
extern result_t packer_test(void);

int main(int argc, const char *argv[])
{
  result_t rc;
  int ntested = 0;
  int npassed = 0;

#define TEST(name) \
  printf(">>>\n"); \
  printf(">>> test: %s\n", #name); \
  printf(">>>\n"); \
  rc = name##_test(); \
  ntested++; \
  if (rc != result_TEST_PASSED) { \
    printf("*** failure\n"); \
  } else { \
    npassed++; \
    printf("<<<\n"); \
    printf("<<< (done)\n"); \
    printf("<<<\n"); \
  }

  /* datastruct */
  TEST(array)
  TEST(atom)
  TEST(bitarr)
  TEST(bitvec)
  TEST(hash)
  TEST(list)
  TEST(ntree)
  TEST(vector)

  /* database */
  TEST(pickle)
  TEST(tagdb)

  /* geom */
  TEST(packer)

  printf("=== %d tested, %d passed\n", ntested, npassed);

  exit(EXIT_SUCCESS);
}
