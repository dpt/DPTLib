
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/suppress.h"
#include "utils/array.h"

#include "datastruct/atom.h"
#include "databases/digest-db.h"
#include "databases/tag-db.h"

/* ----------------------------------------------------------------------- */

#define FILENAME "test-tag-db"

/* ----------------------------------------------------------------------- */

static const char *tagnames[] =
{
  "marty", "jennifer", "emmett", "einstein", "lorraine", "george", "biff",
  "fred" /* do not use (to test unused tag case) */
};

static const unsigned char *ids[] =
{
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01",
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02",
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03",
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04",
};

static const char *renames[] =
{
  "marty.mcfly",
  "jennifer.parker",
  "dr.emmett.brown",
  "einstein", /* no change */
  "lorraine.baines",
  "george.mcfly",
  "biff.tannen",
};

static const struct
{
  int id;
  int tag;
}
taggings[] =
{
  { 0, 0 },
  { 0, 1 },
  { 1, 2 },
  { 2, 3 },
  { 3, 4 },
  { 3, 5 },
  { 4, 0 },
  { 4, 1 },
  { 4, 2 },
  { 4, 3 },
  { 4, 4 },
  { 4, 5 },
  { 4, 6 },
};

/* ----------------------------------------------------------------------- */

typedef struct
{
  tagdb_tag_t  tags[8]; /* 8 == NELEMS(tagnames) */
  tagdb_t     *db;
}
State_t;

/* ----------------------------------------------------------------------- */

static result_t test_open(State_t *state)
{
  result_t err;

  err = tagdb_open(FILENAME, &state->db);

  return err;
}

static result_t test_add_tags(State_t *state)
{
  result_t err;
  int   i;

  for (i = 0; i < NELEMS(tagnames); i++)
  {
    printf("adding '%s'...", tagnames[i]);
    err = tagdb_add(state->db, tagnames[i], &state->tags[i]);
    if (err)
      goto Failure;

    printf("is tag %d\n", state->tags[i]);
  }

  return result_OK;


Failure:

  return err;
}

static result_t test_rename_tags(State_t *state)
{
  result_t err;
  int   i;

  for (i = 0; i < NELEMS(renames); i++)
  {
    char buf[256];

    err = tagdb_tagtoname(state->db, state->tags[i], buf, NULL, sizeof(buf));
    if (err)
      goto Failure;

    printf("renaming '%s'...", buf);

    err = tagdb_rename(state->db, state->tags[i], renames[i]);
    if (err)
      goto Failure;

    err = tagdb_tagtoname(state->db, state->tags[i], buf, NULL, sizeof(buf));
    if (err)
      goto Failure;

    printf("to '%s'\n", buf);
  }

  return result_OK;


Failure:

  return err;
}

static result_t test_enumerate_tags(State_t *state)
{
  result_t err;
  int   cont;
  char  buf[256];

  cont = 0;
  do
  {
    tagdb_tag_t tag;
    int         count;

    printf("continuation %d...", cont);

    err = tagdb_enumerate_tags(state->db, &cont, &tag, &count);
    if (err)
      goto Failure;

    if (cont)
    {
      printf("tag %d, count %d", tag, count);

      err = tagdb_tagtoname(state->db, tag, buf, NULL, sizeof(buf));
      if (err)
        goto Failure;

      printf(" ('%s')", buf);
    }

    printf("\n");
  }
  while (cont);

  return result_OK;


Failure:

  return err;
}

static result_t test_tag_id(State_t *state)
{
  result_t err;
  int   i;

  for (i = 0; i < NELEMS(taggings); i++)
  {
    int whichf;
    int whicht;

    whichf = taggings[i].id;
    whicht = taggings[i].tag;

    printf("tagging id '%d' with %d\n", whichf, state->tags[whicht]);

    err = tagdb_tagid(state->db, ids[whichf], state->tags[whicht]);
    if (err)
      goto Failure;
  }

  return result_OK;


Failure:

  return err;
}

static result_t test_get_tags_for_id(State_t *state)
{
  result_t err;
  int   i;
  int   cont;
  char  buf[256];

  for (i = 0; i < NELEMS(ids); i++)
  {
    int         ntags;
    tagdb_tag_t tag;

    printf("getting tags for id '%d'... ", i);

    ntags = 0;
    cont  = 0;
    do
    {
      err = tagdb_get_tags_for_id(state->db, ids[i], &cont, &tag);
      if (err == result_TAGDB_UNKNOWN_ID)
      {
        printf("id not found ");
        continue;
      }
      else if (err)
      {
        goto Failure;
      }

      if (cont)
      {
        err = tagdb_tagtoname(state->db, tag, buf, NULL, sizeof(buf));
        if (err)
          goto Failure;

        printf("%d ('%s') ", tag, buf);
        ntags++;
      }
    }
    while (cont);

    printf("(%d tags) \n", ntags);
  }

  return result_OK;


Failure:

  return err;
}

static void printdigest(const unsigned char *digest)
{
  int j;

  for (j = 0; j < digestdb_DIGESTSZ; j++)
    printf("%02x", digest[j]);
}

static result_t test_enumerate_ids(State_t *state)
{
  result_t err;
  int      cont;
  char     buf[256];

  cont = 0;
  do
  {
    err = tagdb_enumerate_ids(state->db, &cont, buf, sizeof(buf));
    if (err)
      goto Failure;

    if (cont)
    {
      printf("- ");
      printdigest(buf);
      printf("\n");
    }
  }
  while (cont);

  return result_OK;


Failure:

  return err;
}

static result_t test_enumerate_ids_by_tag(State_t *state)
{
  result_t err;
  int      i;

  for (i = 0; i < NELEMS(tagnames); i++)
  {
    int  cont;
    char buf[256];

    err = tagdb_tagtoname(state->db, state->tags[i], buf, NULL, sizeof(buf));
    if (err)
      goto Failure;

    printf("ids tagged with tag id %d ('%s')...\n", i, buf);

    cont = 0;
    do
    {
      err = tagdb_enumerate_ids_by_tag(state->db, i, &cont,
                                       buf, sizeof(buf));
      if (err)
        goto Failure;

      if (cont)
      {
        printf("- ");
        printdigest(buf);
        printf("\n");
      }
    }
    while (cont);
  }

  return result_OK;


Failure:

  return err;
}

static result_t test_enumerate_ids_by_tags(State_t *state)
{
  result_t err;
  int      cont;
  char     buf[256];

  {
    static const tagdb_tag_t want[] = { 0, 1 };

    printf("ids tagged with '%s' and '%s'...\n",
           tagnames[want[0]], tagnames[want[1]]);

    cont = 0;
    do
    {
      err = tagdb_enumerate_ids_by_tags(state->db, want, NELEMS(want),
                                        &cont, buf, sizeof(buf));
      if (err)
        goto Failure;

      if (cont)
      {
        printf("- ");
        printdigest(buf);
        printf("\n");
      }
    }
    while (cont);
  }

  return result_OK;


Failure:

  return err;
}

static result_t test_tag_remove(State_t *state)
{
  int i;

  for (i = 0; i < NELEMS(state->tags); i++)
    tagdb_remove(state->db, state->tags[i]);

  return result_OK;
}

static result_t test_commit(State_t *state)
{
  result_t err;

  err = tagdb_commit(state->db);
  if (err)
    return err;

  return result_OK;
}

static result_t test_forget(State_t *state)
{
  int i;

  for (i = 0; i < NELEMS(ids); i++)
    tagdb_forget(state->db, ids[i]);

  return result_OK;
}

static result_t test_close(State_t *state)
{
  tagdb_close(state->db); /* remember that this calls _commit */

  return result_OK;
}

static result_t test_delete(State_t *state)
{
  NOT_USED(state);

  tagdb_delete(FILENAME);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static int rnd(int mod)
{
  return (rand() % mod) + 1;
}

static const char *randomtagname(void)
{
  static char buf[5 + 1];

  int length;
  int i;

  length = rnd(NELEMS(buf) - 1);

  for (i = 0; i < length; i++)
    buf[i] = (char) ('a' + rnd(26) - 1);

  buf[i] = '\0';

  return buf;
}

static const unsigned char *randomid(void)
{
  static unsigned char buf[digestdb_DIGESTSZ];

  int i;

  for (i = 0; i < digestdb_DIGESTSZ; i++)
    buf[i] = (char) rnd(256);

  return buf;
}

static result_t bash_enumerate(State_t *state)
{
  result_t err;
  int      cont;
  char     buf[256];

  cont = 0;
  do
  {
    err = tagdb_enumerate_ids(state->db, &cont, buf, sizeof(buf));
    if (err)
      goto failure;

    if (cont)
    {
      int         ntags;
      tagdb_tag_t tag;
      int         cont2;
      char        buf2[256];

      printf("getting tags for '");
      printdigest(buf);
      printf("' ... ");

      ntags = 0;
      cont2 = 0;
      do
      {
        err = tagdb_get_tags_for_id(state->db, buf, &cont2, &tag);
        if (err == result_TAGDB_UNKNOWN_ID)
        {
          printf("id not found ");
          continue;
        }
        else if (err)
        {
          goto failure;
        }

        if (cont2)
        {
          err = tagdb_tagtoname(state->db, tag, buf2, NULL, sizeof(buf2));
          if (err)
            goto failure;

          printf("%d ('%s') ", tag, buf2);
          ntags++;
        }
      }
      while (cont2);

      printf("(%d tags) \n", ntags);
    }
  }
  while (cont);

  return result_OK;


failure:

  return err;
}

static result_t test_bash(State_t *state)
{
  const int ntags       = 100;   /* number of tags to generate */
  const int nids        = 10;    /* number of IDs to generate */
  const int ntaggings   = ntags; /* number of times to tag */
  const int nuntaggings = ntags; /* number of times to untag */
  const int reps        = 3;     /* overall number of repetitions */

  result_t   err;
  int          i;
  tagdb_tag_t  tags[ntags];
  char        *tagnames[ntags];
  char        *idnames[nids];
  int          j;

  srand(0x6487ED51);

  printf("bash: open\n");

  err = tagdb_open(FILENAME, &state->db);
  if (err)
    goto failure;

  printf("bash: setup\n");

  for (i = 0; i < ntags; i++)
    tagnames[i] = NULL;

  for (i = 0; i < nids; i++)
    idnames[i] = NULL;

  for (j = 0; j < reps; j++)
  {
    printf("bash: starting loop %d\n", j);

    printf("bash: create and add tags\n");

    for (i = 0; i < ntags; i++)
    {
      const char *name;
      int         k;

      if (tagnames[i])
        continue;

      do
      {
        name = randomtagname();

        /* ensure that the random name is unique */
        for (k = 0; k < ntags; k++)
          if (tagnames[k] && strcmp(tagnames[k], name) == 0)
            break;
      }
      while (k < ntags);

      tagnames[i] = strdup(name); // FIXME was str_dup
      if (tagnames[i] == NULL)
      {
        err = result_OOM;
        goto failure;
      }

      printf("adding '%s'...", tagnames[i]);

      err = tagdb_add(state->db, tagnames[i], &tags[i]);
      if (err)
        goto failure;

      printf("is tag %d\n", tags[i]);
    }

    printf("bash: create ids\n");

    for (i = 0; i < nids; i++)
    {
      const unsigned char *id;
      int                  k;

      if (idnames[i])
        continue;

      do
      {
        id = randomid();

        /* ensure that the random name is unique */
        for (k = 0; k < nids; k++)
          if (idnames[k] && memcmp(idnames[k], id, digestdb_DIGESTSZ) == 0)
            break;
      }
      while (k < nids);

      idnames[i] = malloc(digestdb_DIGESTSZ);
      if (idnames[i] == NULL)
      {
        err = result_OOM;
        goto failure;
      }

      memcpy(idnames[i], id, digestdb_DIGESTSZ);

      printf("%d is id '", i);
      printdigest(idnames[i]);
      printf("'\n");
    }

    printf("bash: tag random ids with random tags randomly\n");

    for (i = 0; i < ntaggings; i++)
    {
      int whichid;
      int whichtag;

      do
        whichid = rnd(nids) - 1;
      while (idnames[whichid] == NULL);

      do
        whichtag = rnd(ntags) - 1;
      while (tagnames[whichtag] == NULL);

      printf("tagging '");
      printdigest(idnames[whichid]);
      printf("' with %d\n", tags[whichtag]);

      err = tagdb_tagid(state->db, idnames[whichid], tags[whichtag]);
      if (err)
        goto failure;
    }

    printf("bash: enumerate\n");

    err = bash_enumerate(state);
    if (err)
      goto failure;

    printf("bash: rename all tags\n");

    for (i = 0; i < ntags; i++)
    {
      char        buf[256];
      const char *tagname;

      err = tagdb_tagtoname(state->db, tags[i], buf, NULL, sizeof(buf));
      if (err)
        goto failure;

      /* sometimes when renaming we'll try to rename it to a single character
       * name which we've already used, in which case we'll clash. cope with
       * that by re-trying. */

      for (;;)
      {
        tagname = randomtagname();

        free(tagnames[i]);

        tagnames[i] = strdup(tagname); // FIXME was str_dup
        if (tagnames[i] == NULL)
        {
          err = result_OOM;
          goto failure;
        }

        printf("renaming '%s' to '%s'", buf, tagnames[i]);

        err = tagdb_rename(state->db, tags[i], tagnames[i]);
        if (err == result_OK)
          break;

        if (err != result_ATOM_NAME_EXISTS)
          goto failure;

        printf("..name clash..");
      }

      err = tagdb_tagtoname(state->db, tags[i], buf, NULL, sizeof(buf));
      if (err)
        goto failure;

      assert(strcmp(tagnames[i], buf) == 0);

      printf("..ok\n");
    }

    printf("bash: enumerate\n");

    err = bash_enumerate(state);
    if (err)
      goto failure;

    printf("bash: untag random ids with random tags randomly\n");

    for (i = 0; i < nuntaggings; i++)
    {
      int whichid;
      int whichtag;

      do
        whichid = rnd(nids) - 1;
      while (idnames[whichid] == NULL);

      do
        whichtag = rnd(ntags) - 1;
      while (tagnames[whichtag] == NULL);

      printf("untagging '");
      printdigest(idnames[whichid]);
      printf("' with %d\n", tags[whichtag]);

      err = tagdb_untagid(state->db, idnames[whichid], tags[whichtag]);
      if (err)
        goto failure;
    }

    printf("bash: enumerate\n");

    err = bash_enumerate(state);
    if (err)
      goto failure;

    printf("bash: delete every other tag\n");

    for (i = 0; i < ntags; i += 2)
    {
      printf("removing tag %d ('%s')\n", tags[i], tagnames[i]);

      free(tagnames[i]);
      tagnames[i] = NULL;

      tagdb_remove(state->db, tags[i]);
      tags[i] = -1;
    }

    printf("bash: enumerate\n");

    err = bash_enumerate(state);
    if (err)
      goto failure;

    printf("bash: delete every other id\n");

    for (i = 0; i < nids; i += 2)
    {
      printf("removing id %d '", i);
      printdigest(idnames[i]);
      printf("'\n");

      tagdb_forget(state->db, idnames[i]);

      free(idnames[i]);
      idnames[i] = NULL;
    }

    printf("bash: enumerate\n");

    err = bash_enumerate(state);
    if (err)
      goto failure;
  }

  for (i = 0; i < nids; i++)
    free(idnames[i]);

  for (i = 0; i < ntags; i++)
    free(tagnames[i]);

  tagdb_close(state->db); /* remember that this calls _commit */

  tagdb_delete(FILENAME);

  return result_OK;


failure:

  return err;
}

/* ----------------------------------------------------------------------- */

typedef result_t testfn_t(State_t *state);

typedef struct Test
{
  testfn_t   *fn;
  const char *desc;
}
Test_t;

result_t tagdb_test(void); /* suppress "No previous prototype" warning */

result_t tagdb_test(void)
{
  static const Test_t tests[] =
  {
    { test_open,
      "open" },
    { test_add_tags,
      "add tags" },
    { test_rename_tags,
      "rename tags" },
    { test_tag_id,
      "tag id" },
    { test_enumerate_tags,
      "enumerate tags & tag to name" },
    { test_get_tags_for_id,
      "get tags for id" },
    { test_enumerate_ids,
      "enumerate all ids" },
    { test_enumerate_ids_by_tag,
      "enumerate ids by tag" },
    { test_enumerate_ids_by_tags,
      "enumerate ids by tags" },
    { test_commit,
      "commit" },
    { test_tag_remove,
      "remove all tags" },
    { test_get_tags_for_id,
      "get tags for id" },
    { test_forget,
      "forget" },
    { test_get_tags_for_id,
      "get tags for id" },
    { test_close,
      "close" },
    { test_delete,
      "delete" },
    { test_bash,
      "bash" },
  };

  result_t err;
  State_t  state;
  int      i;

  printf("test: init\n");

  err = tagdb_init();
  if (err)
    goto Failure;

  for (i = 0; i < NELEMS(tests); i++)
  {
    printf("test: %s\n", tests[i].desc);

    err = tests[i].fn(&state);
    if (err)
      goto Failure;
  }

  tagdb_fin();

  return result_TEST_PASSED;


Failure:

  printf("\n\n*** Error %x\n", err);

  return result_TEST_FAILED;
}
