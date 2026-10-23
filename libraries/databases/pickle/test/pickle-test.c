/* databases/pickle/test/pickle-test.c */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "datastruct/hash.h"
#include "utils/array.h"

#include "databases/pickle.h"
#include "databases/pickle-reader-hash.h"
#include "databases/pickle-writer-hash.h"

#include "test/all-tests.h"

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

static result_t test1_format_key(const void *key,
                                 char       *buf,
                                 size_t      len,
                                 void       *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  strcpy(buf, key);

  return result_OK;
}

static result_t test1_format_value(const void *key,
                                   char       *buf,
                                   size_t      len,
                                   void       *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  strcpy(buf, key);

  return result_OK;
}

static const pickle_format_methods_t formatters =
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
  err = hash_create(NULL, 20, NULL, NULL, NULL, NULL, &d);
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
    {
      free(s);
      free(v);

      goto Failure;
    }
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
cheese_country_t;

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
cheese_region_t;

typedef enum cheese_source
{
  None,
  Cow,
  Ewe,
  Sheep,
  cheese_SOURCE__LIMIT
}
cheese_source_t;

typedef enum cheese_pasteurised
{
  Pasteurised,
  Unpasteurised,
  cheese_PASTEURISED__LIMIT
}
cheese_pasteurised_t;

/* ----------------------------------------------------------------------- */

static const char *countries[cheese_COUNTRY__LIMIT] =
{
  "England",
  "Scotland",
  "Wales",
};

static const char *cheese_country_to_string(cheese_country_t c)
{
  if ((unsigned int) c >= cheese_COUNTRY__LIMIT)
    return "Unknown country";

  return countries[c];
}

static cheese_country_t cheese_country_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_COUNTRY__LIMIT; i++)
    if (strcmp(countries[i], s) == 0)
      return (cheese_country_t) i;

  return (cheese_country_t) -1;
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

static const char *cheese_region_to_string(cheese_region_t r)
{
  if ((unsigned int) r >= cheese_REGION__LIMIT)
    return "Unknown region";

  return regions[r];
}

static cheese_region_t cheese_region_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_REGION__LIMIT; i++)
    if (strcmp(regions[i], s) == 0)
      return (cheese_region_t) i;

  return (cheese_region_t) -1;
}

static const char *sources[cheese_SOURCE__LIMIT] =
{
  "None",
  "Cow",
  "Ewe",
  "Sheep",
};

static const char *cheese_source_to_string(cheese_source_t s)
{
  if ((unsigned int) s >= cheese_SOURCE__LIMIT)
    return "Unknown source";

  return sources[s];
}

static cheese_source_t cheese_source_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_SOURCE__LIMIT; i++)
    if (strcmp(sources[i], s) == 0)
      return (cheese_source_t) i;

  return (cheese_source_t) -1;
}

static const char *pasteurisations[cheese_PASTEURISED__LIMIT] =
{
  "Pasteurised",
  "Unpasteurised",
};

static const char *cheese_pasteurised_to_string(cheese_pasteurised_t p)
{
  if ((unsigned int) p >= cheese_PASTEURISED__LIMIT)
    return "Unknown pasteurisation";

  return pasteurisations[p];
}

static cheese_pasteurised_t cheese_pasteurised_from_string(const char *s)
{
  int i;

  for (i = 0; i < cheese_PASTEURISED__LIMIT; i++)
    if (strcmp(pasteurisations[i], s) == 0)
      return (cheese_pasteurised_t) i;

  return (cheese_pasteurised_t) -1;
}

/* ----------------------------------------------------------------------- */

typedef struct cheese_key
{
  char name[32];
}
cheese_key_t;

typedef struct cheese_value
{
  cheese_country_t     country;
  cheese_region_t      region;
  cheese_source_t      source1, source2;
  cheese_pasteurised_t pasteurised;
  int                  age; /* typical age in months */
}
cheese_value_t;

/* ----------------------------------------------------------------------- */

static result_t cheese_format_key(const void *vkey,
                                  char       *buf,
                                  size_t      len,
                                  void       *opaque)
{
  const cheese_key_t *key = vkey;

  NOT_USED(opaque);

  strncpy(buf, key->name, len);
  buf[len - 1] = '\0';

  return result_OK;
}

static result_t cheese_format_value(const void *vvalue,
                                    char       *buf,
                                    size_t      len,
                                    void       *opaque)
{
  const cheese_value_t *value = vvalue;

  NOT_USED(opaque);

  snprintf(buf, len,
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

static const pickle_format_methods_t format_cheese_methods =
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
  cheese_value_t *value;
  int             rc;
  char            country[100];
  char            region[100];
  char            source1[100];
  char            source2[100];
  char            pasteurised[100];
  int             age;

  NOT_USED(len);
  NOT_USED(opaque);

  value = malloc(sizeof(cheese_value_t));
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

  if ((unsigned int) value->country >= cheese_COUNTRY__LIMIT ||
      (unsigned int) value->region >= cheese_REGION__LIMIT   ||
      (unsigned int) value->source1 >= cheese_SOURCE__LIMIT  ||
      (unsigned int) value->source2 >= cheese_SOURCE__LIMIT  ||
      (unsigned int) value->pasteurised >= cheese_PASTEURISED__LIMIT)
  {
    free(value);
    return result_BAD_ARG;
  }

  *pvalue = value;

  return result_OK;
}

static const pickle_unformat_methods_t unformat_cheese_methods =
{
  " -*- ",
  5,
  cheese_unformat_key,
  cheese_unformat_value
};

/* ----------------------------------------------------------------------- */

static const struct
{
  cheese_key_t   key;
  cheese_value_t value;
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

  err = hash_create(NULL,
                    0,
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
   (cheese_value_t *) &cheeses[i].value);
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

  err = hash_create(NULL,
                    0,
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

#define FILENAME3 "testpickle3"

/* Records what pickle_unpickle asks of a writer, and fails on request. */
typedef struct recorder
{
  int  fail_start;
  int  fail_next_at;
  int  started;
  int  stopped;
  int  lines;
  char last[64];
}
recorder_t;

static result_t recorder_start(void *assocarr, void **state, void *opaque)
{
  recorder_t *r = opaque;

  NOT_USED(assocarr);

  if (r->fail_start)
    return result_BAD_ARG;

  r->started++;
  *state = r;

  return result_OK;
}

static void recorder_stop(void *state, void *opaque)
{
  recorder_t *r = opaque;

  NOT_USED(state);

  r->stopped++;
}

static result_t recorder_next(void *state,
                              void *key,
                              void *value,
                              void *opaque)
{
  recorder_t *r = opaque;

  NOT_USED(state);

  if (++r->lines == r->fail_next_at)
    return result_BAD_ARG;

  snprintf(r->last, sizeof(r->last), "%s=%s",
           (const char *) key, (const char *) value);

  return result_OK;
}

static const pickle_writer_methods_t recorder_methods =
{
  recorder_start,
  recorder_stop,
  recorder_next
};

/* Hands back the text in place; refuses "badkey" and "badvalue". */
static result_t text_unformat_key(const char *buf,
                                  size_t      len,
                                  void      **key,
                                  void       *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  if (strcmp(buf, "badkey") == 0)
    return result_BAD_ARG;

  *key = (void *) buf;

  return result_OK;
}

static result_t text_unformat_value(const char *buf,
                                    size_t      len,
                                    void      **value,
                                    void       *opaque)
{
  NOT_USED(len);
  NOT_USED(opaque);

  if (strcmp(buf, "badvalue") == 0)
    return result_BAD_ARG;

  *value = (void *) buf;

  return result_OK;
}

static const pickle_unformat_methods_t unformat_text_methods =
{
  ": ",
  2,
  text_unformat_key,
  text_unformat_value
};

/* Writes 'text' to a file then unpickles it into 'r'. */
static result_t unpickle_text(const char *text, recorder_t *r)
{
  FILE *f;

  f = fopen(FILENAME3, "wb");
  if (f == NULL)
    return result_PICKLE_COULDNT_OPEN_FILE;

  fputs(text, f);
  fclose(f);

  return pickle_unpickle(FILENAME3, NULL, &recorder_methods,
                         &unformat_text_methods, r);
}

static result_t pickle__test3_unpickle(void)
{
  static const struct
  {
    const char *text;
    result_t    want;
  }
  bad[] =
  {
    { "2\n",              result_PICKLE_INCOMPATIBLE },
    { "1\nnosplit\n",     result_PICKLE_SYNTAX_ERROR },
    { "1\nkey: \n",       result_PICKLE_SYNTAX_ERROR },
    { "1\nbadkey: v\n",   result_BAD_ARG             },
    { "1\nk: badvalue\n", result_BAD_ARG             },
  };

  result_t   err;
  recorder_t r;
  int        i;
  char      *text;
  char      *p;

  printf("test: unpickle comments and a last line without a newline\n");

  memset(&r, 0, sizeof(r));
  err = unpickle_text("# comment\n1\na: b\n# another\nc: d", &r);
  if (err || r.started != 1 || r.stopped != 1 || r.lines != 2 ||
      strcmp(r.last, "c=d") != 0)
    goto Failure;

  printf("test: unpickle malformed files\n");

  for (i = 0; i < NELEMS(bad); i++)
  {
    memset(&r, 0, sizeof(r));
    err = unpickle_text(bad[i].text, &r);
    if (err != bad[i].want || r.stopped != 1)
    {
      printf("case %d: error %x, want %x\n", i, err, bad[i].want);
      goto Failure;
    }
  }

  printf("test: unpickle with a failing writer\n");

  memset(&r, 0, sizeof(r));
  r.fail_next_at = 1;
  if (unpickle_text("1\nk: v\n", &r) != result_BAD_ARG || r.stopped != 1)
    goto Failure;

  memset(&r, 0, sizeof(r));
  r.fail_start = 1;
  if (unpickle_text("1\nk: v\n", &r) != result_BAD_ARG || r.stopped != 0)
    goto Failure;

  printf("test: unpickle a missing file\n");

  memset(&r, 0, sizeof(r));
  if (pickle_unpickle("testpickle-missing", NULL, &recorder_methods,
                      &unformat_text_methods, &r) !=
      result_PICKLE_COULDNT_OPEN_FILE)
    goto Failure;

  printf("test: unpickle more lines than fit in the read buffer\n");

  text = malloc(2 + 300 * 16 + 1);
  if (text == NULL)
    goto Failure;

  strcpy(text, "1\n");
  p = text + 2;
  for (i = 0; i < 300; i++)
    p += snprintf(p, 16, "key%03d: val%03d\n", i, i);

  memset(&r, 0, sizeof(r));
  err = unpickle_text(text, &r);
  if (err || r.lines != 300 || strcmp(r.last, "key299=val299") != 0)
  {
    free(text);
    goto Failure;
  }

  printf("test: unpickle a line longer than the read buffer\n");

  /* one key: value line over 2000 characters */
  memset(text, 'x', 2 + 300 * 16);
  memcpy(text, "1\nk: ", 5);
  text[2 + 300 * 16] = '\0';

  memset(&r, 0, sizeof(r));
  err = unpickle_text(text, &r);
  free(text);
  if (err != result_PICKLE_SYNTAX_ERROR)
    goto Failure;

  return result_TEST_PASSED;


Failure:

  printf("\n\n*** Error %x\n", err);

  return result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

result_t pickle_test(const char *resources)
{
  result_t rc;

  NOT_USED(resources);

  printf("test: pickle test 1\n");

  rc = pickle__test1_write();
  if (rc != result_TEST_PASSED)
    goto cleanup;

  printf("test: pickle test 2\n");

  rc = pickle__test2_write();
  if (rc != result_TEST_PASSED)
    goto cleanup;

  rc = pickle__test2_read();
  if (rc != result_TEST_PASSED)
    goto cleanup;

  printf("test: pickle test 3\n");

  rc = pickle__test3_unpickle();

cleanup:

  /* delete the temporary pickle files */
  remove("testpickle1");
  remove(FILENAME);
  remove(FILENAME3);

  return rc;
}
