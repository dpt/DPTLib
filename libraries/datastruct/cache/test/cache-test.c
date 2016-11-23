
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "datastruct/cache.h"

result_t cache_test(void); /* suppress "No previous prototype" warning */
static int cache_test_outer(const cacheconfig_t *config,
                            size_t               length,
                            int                  maxkey);
static int cache_test_put(cache_t *cache, int maxkey);

result_t cache_test(void)
{
  result_t      err = result_OK;
  cacheconfig_t config;
  int           nfailures;
  int           i;

  config.hash_chain_length   = 4;
  config.nentries_percentage = 25;

#define MINLEN 512
#define MAXLEN 1536
#define MAXKEY 970

  nfailures = 0;
  srand(0x060708aa);
  for (i = 0; i < 10; i++)
  {
    size_t   length;

    length = MINLEN + rand() / (RAND_MAX / (MAXLEN - MINLEN + 1) + 1);
    printf("test: cache with length %zu\n", length);
    err = cache_test_outer(&config, length, MAXKEY);
    if (err != result_TEST_PASSED)
    {
      printf("\n\n*** Error %x\n", err);
      nfailures++;
    }
  }

  printf("\n");

  printf("cache: %d failure(s)\n", nfailures);

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}

static result_t cache_test_outer(const cacheconfig_t *config,
                                 size_t               cachelength,
                                 int                  maxkey)
{
  unsigned char  block[cachelength];
  cache_t       *cache;
  result_t       err;

  printf("test: destroy NULL\n");

  cache_destroy(NULL);

  printf("test: create-destroy\n");

  err = cache_create(NULL, cachelength, &cache);
  if (err)
    goto failure;

  cache_destroy(cache);

  printf("test: construct (in provided block)\n");

  err = cache_construct(NULL, block, cachelength, &cache);
  if (err)
    goto failure;

  cache = NULL;

  printf("test: create and run tests\n");

  err = cache_create(config, cachelength, &cache);
  if (err)
    goto failure;

  err = cache_test_put(cache, maxkey);
  if (err != result_TEST_PASSED)
    goto failure;

  cache_destroy(cache);

  printf("test: insert maximum sized block\n");

  {
    cacheinfo_t    info;
    unsigned char *data;
    unsigned int   checksum;
    int            i;
    unsigned int   saved_checksum;
    unsigned char *p;

    err = cache_create(config, cachelength, &cache);
    if (err)
      goto failure;

    cache_get_info(cache, &info);

    data = malloc(info.maxlength);
    if (data == NULL)
    {
      err = result_OOM;
      goto failure;
    }

    checksum = 0;
    for (i = 0; i < (int) info.maxlength; i++)
    {
      data[i] = (unsigned char) i;
      checksum += data[i];
    }
    saved_checksum = checksum;

    err = cache_put(cache, 0xdeadbeef, data, info.maxlength, NULL);
    if (err)
      goto failure;

    p = cache_get(cache, 0xdeadbeef);
    if (p == NULL)
    {
      err = result_TEST_FAILED;
      goto failure;
    }

    checksum = 0;
    for (i = 0; i < (int) info.maxlength; i++)
      checksum += p[i];

    if (saved_checksum != checksum)
    {
      err = result_TEST_FAILED;
      goto failure;
    }

    cache_stats(cache, 0);

    cache_destroy(cache);
  }

  return result_TEST_PASSED;

failure:

  return err;
}

static result_t cache_test_put(cache_t *cache, int maxkey)
{
  char data[100];
  int  i;

  cache_stats(cache, 0);

  printf("test: cache_put %d times\n", maxkey);

  for (i = 0; i < maxkey; i++)
  {
    sprintf(data, "(%d)", i);
    cache_put(cache, i, data, strlen(data) + 1, NULL);
  }

  printf("test: cache_get existing keys %d times\n", maxkey);

  {
    int ncached;

    ncached = 0;
    for (i = 0; i < maxkey; i++)
    {
      char *cached;

      int len = sprintf(data, "(%d)", i);
      cached = (char *) cache_get(cache, i);
      if (cached)
      {
        if (memcmp(data, cached, len + 1) != 0)
          return result_TEST_FAILED; /* cached data didn't match */

        ncached++;
      }
    }

    printf("ncached = %d\n", ncached);
  }

  printf("test: cache_put %d more times\n", maxkey);

  for (i = 0; i < maxkey; i++)
  {
    sprintf(data, "(%d)", i);

    cache_put(cache, i, data, strlen(data) + 1, NULL);
  }

  cache_stats(cache, 0);

  printf("test: cache_empty\n");

  cache_empty(cache);

  return result_TEST_PASSED;
}
