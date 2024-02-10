
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "datastruct/atom.h"

#include "test/all-tests.h"

static const char *data[] =
{
  "deckard", "batty", "rachael", "gaff", "bryant", "pris", "sebastian",

  /* dupes */
  "batty", "bryant", "deckard", "gaff", "pris", "rachael", "sebastian",
};

static const char *newnames[] =
{
  /* last is a dupe */
  "fred", "barney", "wilma", "betty", "pebbles", "bamm bamm", "fred",
};

static result_t test_add(atom_set_t *d)
{
  result_t err;
  int      i;

  printf("test: add\n");

  for (i = 0; i < NELEMS(data); i++)
  {
    atom_t idx;

    printf("adding '%s'... ", data[i]);

    err = atom_new(d, (const unsigned char *) data[i], strlen(data[i]) + 1,
                   &idx);
    if (err && err != result_ATOM_NAME_EXISTS)
      return err;

    if (err == result_ATOM_NAME_EXISTS)
      printf("already exists ");

    printf("as %d\n", idx);
  }

  return result_OK;
}

static result_t test_to_string(atom_set_t *d)
{
  int i;

  printf("test: to string\n");

  for (i = 0; i < NELEMS(data); i++)
  {
    const char *string;

    string = (const char *) atom_get(d, i, NULL);
    if (string)
      printf("%d is '%s'\n", i, string);
  }

  return result_OK;
}

static result_t test_to_string_and_len(atom_set_t *d)
{
  int i;

  printf("test: to string and length\n");

  for (i = 0; i < NELEMS(data); i++)
  {
    const char *string;
    size_t      len;

    string = (const char *) atom_get(d, i, &len);
    if (string)
      printf("%d is '%.*s' (length %zu)\n", i, (int) len, string, len);
  }

  return result_OK;
}

static result_t test_delete(atom_set_t *d)
{
  int i;

  printf("test: delete\n");

  for (i = 0; i < NELEMS(data); i++)
    atom_delete_block(d, (const unsigned char *) data[i], strlen(data[i]) + 1);

  return result_OK;
}

static result_t test_rename(atom_set_t *d)
{
  result_t err;
  int   i;

  printf("test: rename\n");

  for (i = 0; i < NELEMS(newnames); i++)
  {
    atom_t idx;

    printf("adding '%s'... ", data[i]);

    err = atom_new(d, (const unsigned char *) data[i],
                   strlen(data[i]) + 1, &idx);
    if (err && err != result_ATOM_NAME_EXISTS)
      return err;

    if (err == result_ATOM_NAME_EXISTS)
      printf("already exists ");

    printf("as %d\n", idx);

    printf("renaming index %d to '%s'... ", idx, newnames[i]);

    err = atom_set(d, idx, (const unsigned char *) newnames[i],
                   strlen(newnames[i]) + 1);
    if (err == result_ATOM_NAME_EXISTS)
      printf("already exists!");
    else if (err)
      return err;
    else
      printf("ok");

    printf("\n");
  }

  return result_OK;
}

static int rnd(int mod)
{
  return (rand() % mod) + 1;
}

static const char *randomname(void)
{
  static char buf[5 + 1];

  int length;
  int i;

  length = rnd(NELEMS(buf) - 1);

  for (i = 0; i < length; i++)
    buf[i] = (char)('a' + rnd(26) - 1);

  buf[i] = '\0';

  return buf;
}

static result_t test_random(atom_set_t *d)
{
  result_t err;
  int   i;

  printf("test: random\n");

  for (i = 0; i < 100; i++)
  {
    const char *name;
    atom_t  idx;

    name = randomname();

    printf("adding '%s'... ", name);

    err = atom_new(d, (const unsigned char *) name, strlen(name) + 1, &idx);
    if (err && err != result_ATOM_NAME_EXISTS)
      return err;

    if (err == result_ATOM_NAME_EXISTS)
      printf("already exists ");

    printf("as %d\n", idx);
  }

  return result_OK;
}

result_t atom_test(const char *resources)
{
  result_t   err;
  atom_set_t *d;

  NOT_USED(resources);

  d = atom_create_tuned(1, 12);
  if (d == NULL)
    goto Failure;

  err = test_add(d);
  if (err)
    goto Failure;

  err = test_to_string(d);
  if (err)
    goto Failure;

  err = test_to_string_and_len(d);
  if (err)
    goto Failure;

  err = test_delete(d);
  if (err)
    goto Failure;

  err = test_to_string_and_len(d);
  if (err)
    goto Failure;

  err = test_add(d);
  if (err)
    goto Failure;

  err = test_to_string(d);
  if (err)
    goto Failure;

  err = test_rename(d);
  if (err)
    goto Failure;

  err = test_to_string(d);
  if (err)
    goto Failure;

  err = test_to_string_and_len(d);
  if (err)
    goto Failure;

  err = test_random(d);
  if (err)
    goto Failure;

  atom_destroy(d);

  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
