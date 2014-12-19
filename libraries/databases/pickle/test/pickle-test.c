
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/suppress.h"
#include "datastruct/hash.h"
#include "utils/array.h"

#include "databases/pickle.h"
#include "databases/pickle-reader-hash.h"
#include "databases/pickle-writer-hash.h"

/* ----------------------------------------------------------------------- */

static char *my_strdup(const char *s)
{
  size_t l;
  char  *d;

  l = strlen(s);

  d = malloc(l + 1);
  if (d == NULL)
    return NULL;

  memcpy(d, s, l + 1);

  return d;
}

/* ----------------------------------------------------------------------- */

static result_t test1_format_key(const void *key, char *buf, size_t len, void *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  strcpy(buf, key);

  return result_OK;
}

static result_t test1_format_value(const void *key, char *buf, size_t len, void *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  strcpy(buf, key);

  return result_OK;
}

static const pickle_format_methods formatters =
{
  "test comment",
  NELEMS("test comment") - 1,
  ": ",
  2,
  test1_format_key,
  test1_format_value
};

/* ----------------------------------------------------------------------- */

static result_t pickle__test1_write(void)
{
  static const struct
  {
    const char *name;
    const char *value;
  }
  data[] =
  {
    { "deckard",   "rick" },
    { "batty",     "roy" },
    { "tyrell",    "rachael" },
    { "gaff",      "n/a" },
    { "bryant",    "n/a" },
    { "pris",      "n/a" },
    { "sebastian", "jf" },
  };

  result_t err;
  hash_t  *d;
  int      i;

  printf("test: create hash\n");

  /* use default string handling */
  err = hash_create(20, NULL, NULL, NULL, NULL, &d);
  if (err)
    goto Failure;

  printf("test: insert\n");

  for (i = 0; i < NELEMS(data); i++)
  {
    char *s;
    char *v;

    printf("adding '%s':'%s'...\n", data[i].name, data[i].value);

    s = my_strdup(data[i].name);
    v = my_strdup(data[i].value);

    if (!s || !v)
    {
      free(s);
      free(v);

      err = result_OOM;
      goto Failure;
    }

    err = hash_insert(d, s, v);
    if (err)
      goto Failure;
  }

  err = pickle_pickle("testpickle1", d, &pickle_reader_hash, &formatters, NULL);
  if (err)
    goto Failure;

  printf("test: destroy hash\n");

  hash_destroy(d);

  return result_TEST_PASSED;


Failure:

  printf("\n\n*** Error %x\n", err);

  return result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

typedef enum cheese_country
{
  England,
  Scotland,
  Wales,
  cheese_COUNTRY__LIMIT
}
cheese_country;

typedef enum cheese_region
{
  Derbyshire,
  Lanarkshire,
  Leicestershire,
  MidGlamorgan,
  Somerset,
  Yorkshire,
  cheese_REGION__LIMIT
}
cheese_region;

typedef enum cheese_source
{
  None,
  Cow,
  Ewe,
  Sheep,
  cheese_SOURCE__LIMIT
}
cheese_source;

typedef enum cheese_pasteurised
{
  Pasteurised,
  Unpasteurised,
  cheese_PASTEURISED__LIMIT
}
cheese_pasteurised;

/* ----------------------------------------------------------------------- */

static const char *countries[cheese_COUNTRY__LIMIT] =
{
  "England",
  "Scotland",
  "Wales",
};

static const char *cheese_country_to_string(cheese_country c)
{
  if (c < 0 || c >= cheese_COUNTRY__LIMIT)
    return "Unknown country";

  return countries[c];
}

static cheese_country cheese_country_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_COUNTRY__LIMIT; i++)
    if (strcmp(countries[i], s) == 0)
      return (cheese_country) i;

  return (cheese_country) -1;
}

static const char *regions[cheese_REGION__LIMIT] =
{
  "Derbyshire",
  "Lanarkshire",
  "Leicestershire",
  "MidGlamorgan",
  "Somerset",
  "Yorkshire",
};

static const char *cheese_region_to_string(cheese_region r)
{
  if (r < 0 || r >= cheese_REGION__LIMIT)
    return "Unknown region";

  return regions[r];
}

static cheese_region cheese_region_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_REGION__LIMIT; i++)
    if (strcmp(regions[i], s) == 0)
      return (cheese_region) i;

  return (cheese_region) -1;
}

static const char *sources[cheese_SOURCE__LIMIT] =
{
  "None",
  "Cow",
  "Ewe",
  "Sheep",
};

static const char *cheese_source_to_string(cheese_source s)
{
  if (s < 0 || s >= cheese_SOURCE__LIMIT)
    return "Unknown source";

  return sources[s];
}

static cheese_source cheese_source_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_SOURCE__LIMIT; i++)
    if (strcmp(sources[i], s) == 0)
      return (cheese_source) i;

  return (cheese_source) -1;
}

static const char *pasteurisations[cheese_PASTEURISED__LIMIT] =
{
  "Pasteurised",
  "Unpasteurised",
};

static const char *cheese_pasteurised_to_string(cheese_pasteurised p)
{
  if (p < 0 || p >= cheese_PASTEURISED__LIMIT)
    return "Unknown pasteurisation";

  return pasteurisations[p];
}

static cheese_pasteurised cheese_pasteurised_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_PASTEURISED__LIMIT; i++)
    if (strcmp(pasteurisations[i], s) == 0)
      return (cheese_pasteurised) i;

  return (cheese_pasteurised) -1;
}

/* ----------------------------------------------------------------------- */

typedef struct cheese_key
{
  char name[32];
}
cheese_key;

typedef struct cheese_value
{
  cheese_country     country;
  cheese_region      region;
  cheese_source      source1, source2;
  cheese_pasteurised pasteurised;
  int                age; /* typical age in months */
}
cheese_value;

/* ----------------------------------------------------------------------- */

static result_t cheese_format_key(const void *vkey, char *buf, size_t len, void *opaque)
{
  const cheese_key *key = vkey;

  NOT_USED(opaque);

  strncpy(buf, key->name, len);
  buf[len - 1] = '\0';

  return result_OK;
}

static result_t cheese_format_value(const void *vvalue, char *buf, size_t len, void *opaque)
{
  const cheese_value *value = vvalue;

  NOT_USED(opaque);

  sprintf(buf,
         "%s %s %s %s %s %d",
          cheese_country_to_string(value->country),
          cheese_region_to_string(value->region),
          cheese_source_to_string(value->source1),
          cheese_source_to_string(value->source2),
          cheese_pasteurised_to_string(value->pasteurised),
          value->age);
  buf[len - 1] = '\0';

  return result_OK;
}

static const pickle_format_methods format_cheese_methods =
{
  "Cheeses",
  NELEMS("Cheeses") - 1,
  " -*- ",
  5,
  cheese_format_key,
  cheese_format_value
};

/* ----------------------------------------------------------------------- */

static result_t cheese_unformat_key(const char *buf,
                                    size_t      len,
                                    void      **key,
                                    void       *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  *key = my_strdup(buf);
  if (*key == NULL)
    return result_OOM;

  return result_OK;
}

static result_t cheese_unformat_value(const char *buf,
                                      size_t      len,
                                      void      **pvalue,
                                      void       *opaque)
{
  cheese_value *value;
  int           rc;
  char          country[100];
  char          region[100];
  char          source1[100];
  char          source2[100];
  char          pasteurised[100];
  int           age;

  NOT_USED(len);
  NOT_USED(opaque);

  value = malloc(sizeof(cheese_value));
  if (value == NULL)
    return result_OOM;

  rc = sscanf(buf,
             "%s %s %s %s %s %d",
              country,
              region,
              source1,
              source2,
              pasteurised,
             &age);
  if (rc == EOF || rc != 6)
  {
    free(value);
    return result_BAD_ARG;
  }

  value->country     = cheese_country_from_string(country);
  value->region      = cheese_region_from_string(region);
  value->source1     = cheese_source_from_string(source1);
  value->source2     = cheese_source_from_string(source2);
  value->pasteurised = cheese_pasteurised_from_string(pasteurised);
  value->age         = age;

  if (value->country     < 0 ||
      value->region      < 0 ||
      value->source1     < 0 ||
      value->source2     < 0 ||
      value->pasteurised < 0)
    return result_BAD_ARG;

  *pvalue = value;

  return result_OK;
}

static const pickle_unformat_methods unformat_cheese_methods =
{
  " -*- ",
  5,
  cheese_unformat_key,
  cheese_unformat_value
};

/* ----------------------------------------------------------------------- */

static const struct
{
  cheese_key   key;
  cheese_value value;
}
cheeses[] =
{
  { { "Caerphilly"    }, { Wales,    MidGlamorgan,   Cow,   None, Pasteurised,   2 } },
  { { "Cheddar"       }, { England,  Somerset,       Cow,   None, Pasteurised,   3 } },
  { { "Lanark Blue"   }, { Scotland, Lanarkshire,    Sheep, None, Unpasteurised, 3 } },
  { { "Red Leicester" }, { England,  Leicestershire, Cow,   None, Pasteurised,   6 } },
  { { "Stilton"       }, { England,  Derbyshire,     Cow,   None, Pasteurised,   2 } },
  { { "Wensleydale"   }, { England,  Yorkshire,      Cow,   Ewe,  Pasteurised,   3 } },
};

/* ----------------------------------------------------------------------- */

/* Destroy the key. */
static void cheese_key_destroy(void *value)
{
  free(value);
}

/* Destroy the value. */
static void cheese_value_destroy(void *value)
{
  free(value);
}

/* ----------------------------------------------------------------------- */

#define FILENAME "testpickle2"

static result_t pickle__test2_write(void)
{
  result_t err;
  hash_t  *d;
  int      i;

  printf("test: create hash\n");

  err = hash_create(0,
                    NULL,
                    NULL,
                    hash_no_destroy_key,
                    hash_no_destroy_value,
                   &d);
  if (err)
    goto Failure;

  printf("test: insert\n");

  for (i = 0; i < NELEMS(cheeses); i++)
  {
    printf("adding '%s'...\n", cheeses[i].key.name);

    err = hash_insert(d,
             (char *) cheeses[i].key.name,
    (cheese_value *) &cheeses[i].value);
    if (err)
      goto Failure;
  }

  printf("test: pickle\n");

  err = pickle_pickle(FILENAME,
                      d,
                     &pickle_reader_hash,
                     &format_cheese_methods,
                      NULL);
  if (err)
    goto Failure;

  printf("test: destroy hash\n");

  hash_destroy(d);

  return result_TEST_PASSED;


Failure:

  printf("\n\n*** Error %x\n", err);

  return result_TEST_FAILED;
}

static int my_walk_fn(const void *key, const void *value, void *opaque)
{
  char kbuf[256];
  char vbuf[256];

  NOT_USED(opaque);

  cheese_format_key(key, kbuf, sizeof(kbuf), NULL);
  cheese_format_value(value, vbuf, sizeof(vbuf), NULL);

  printf("walk '%s':'%s'...\n", kbuf, vbuf);

  return 0;
}

static result_t pickle__test2_read(void)
{
  result_t err;
  hash_t  *d;

  printf("test: create hash\n");

  err = hash_create(0,
                    NULL,
                    NULL,
                    cheese_key_destroy,
                    cheese_value_destroy,
                   &d);
  if (err)
    goto Failure;

  printf("test: unpickle\n");

  err = pickle_unpickle(FILENAME,
                        d,
                       &pickle_writer_hash,
                       &unformat_cheese_methods,
                        NULL);
  if (err)
    goto Failure;

  printf("test: iterate\n");

  hash_walk(d, my_walk_fn, NULL);

  printf("test: destroy hash\n");

  hash_destroy(d);

  return result_TEST_PASSED;


Failure:

  printf("\n\n*** Error %x\n", err);

  return result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

result_t pickle_test(void)
{
  result_t rc;

  printf("test: pickle test 1\n");

  rc = pickle__test1_write();
  if (rc != result_TEST_PASSED)
    return rc;

  printf("test: pickle test 2\n");

  rc = pickle__test2_write();
  if (rc != result_TEST_PASSED)
    return rc;

  rc = pickle__test2_read();
  if (rc != result_TEST_PASSED)
    return rc;

  return result_TEST_PASSED;
}
