
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "utils/array.h"
#include "utils/minmax.h"

#include "test/txtscr.h"

#include "geom/packer.h"

/* ----------------------------------------------------------------------- */

static result_t drawbox(const box_t *box, void *opaque)
{
  txtscr_t *scr = opaque;

  printf("drawbox: %d %d %d %d\n", box->x0, box->y0, box->x1, box->y1);

  txtscr_addbox(scr, box);

  return result_OK;
}

static result_t dumppacker(packer_t *packer, txtscr_t *scr)
{
  result_t err;

  txtscr_clear(scr);

  err = packer_map(packer, drawbox, scr);
  if (err)
    return err;

  txtscr_print(scr);

  return result_OK;
}

static result_t dumpboxlist(const box_t *list, int nelems, txtscr_t *scr)
{
  result_t         err;
  const box_t *b;

  txtscr_clear(scr);

  for (b = list; b < list + nelems; b++)
  {
    err = drawbox(b, scr);
    if (err)
      return err;
  }

  txtscr_print(scr);

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static result_t subtest1(packer_t *packer, txtscr_t *scr)
{
  static const box_t areas[] =
  {
    {  0,  0, 10, 20 },
    { 40,  0, 50, 20 },
    { 10, 20, 40, 25 },
  };

  result_t err;
  int   i;

  /* place a large area */

  for (i = 0; i < NELEMS(areas); i++)
  {
    err = packer_place_at(packer, &areas[i]);
    if (err)
      goto failure;
  }

  printf("\n");

  for (i = 0; i < 10; i++)
  {
    const box_t *placed;

    dumppacker(packer, scr);

    err = packer_place_by(packer, packer_LOC_TOP_LEFT, 3, 3, &placed);
    if (err == result_PACKER_DIDNT_FIT)
    {
      printf("could not place\n");
      break;
    }

    printf("%d placed at: {%d %d %d %d}\n",
           i,
           placed->x0,
           placed->y0,
           placed->x1,
           placed->y1);
  }

  dumppacker(packer, scr);

  packer_clear(packer, packer_CLEAR_LEFT);

  dumppacker(packer, scr);

  return result_OK;


failure:

  return err;
}

static result_t test1(void)
{
  static const box_t pagedims = { 0, 0, 50, 25 };

  result_t     err;
  txtscr_t *scr;
  packer_t *packer;
  box_t    margins;

  printf("test1\n");

  scr = txtscr_create(50, 25);
  if (scr == NULL)
  {
    err = result_OOM;
    goto failure;
  }

  packer = packer_create(&pagedims);
  if (packer == NULL)
  {
    err = result_OOM;
    goto failure;
  }

  err = subtest1(packer, scr);
  if (err)
    goto failure;

  packer_destroy(packer);

  printf("\n\nwith margins\n\n\n");

  packer = packer_create(&pagedims);
  if (packer == NULL)
  {
    err = result_OOM;
    goto failure;
  }

  margins.x0 = 5;
  margins.y0 = 3;
  margins.x1 = 5;
  margins.y1 = 3;
  packer_set_margins(packer, &margins);

  err = subtest1(packer, scr);
  if (err)
    goto failure;

  margins.x0 = 0;
  margins.y0 = 0;
  margins.x1 = 0;
  margins.y1 = 0;
  packer_set_margins(packer, &margins);

  dumppacker(packer, scr);

  packer_destroy(packer);

  txtscr_destroy(scr);

  return result_OK;


failure:

  return err;
}

#define MAXWIDTH  100
#define MAXHEIGHT 36
#define PADDING   2

static result_t subtest2(packer_loc_t loc, packer_cleardir_t clear)
{
  static const box_t pagedims = {       0,       0, MAXWIDTH, MAXHEIGHT };
  static const box_t margins  = { PADDING, PADDING,  PADDING,   PADDING };

  struct
  {
    int minw;
    int maxw;
    int h;
    int chosenw;
  }
  elements[] =
  {
    { 26, 26,     26, 0 },
    { 26, 26,     28, 0 },
//  { 96,INT_MAX, 1, 0 },
    { 26, 26,     26, 0 },
    // want to force a newline here
    { 26, INT_MAX, 2, 0 }
  };

  result_t         err;
  txtscr_t     *scr;
  packer_t     *packer;
  int           i;
  int           j;
  const box_t *placed;
  box_t        list[32];

  printf("test2: loc=%d, clear=%d\n", loc, clear);

  scr = txtscr_create(MAXWIDTH, MAXHEIGHT);
  if (scr == NULL)
  {
    err = result_OOM;
    goto failure;
  }

  packer = packer_create(&pagedims);
  if (packer == NULL)
  {
    err = result_OOM;
    goto failure;
  }

  packer_set_margins(packer, &margins);

  dumppacker(packer, scr);

  i = 0;
  j = 0;

  do
  {
    int remaining;
    int first, last;
    int need;
    int k;

    printf("loop: i=%d\n", i);

    remaining = packer_next_width(packer, loc);
    printf("remaining = %d\n", remaining);

    first = last = i; /* upper bound 'last' is exclusive */

    need = elements[last].minw; /* skip padding on initial element */
    while (remaining >= need)
    {
      int chosenw;

      chosenw = MIN(remaining, elements[last].maxw);
      elements[last].chosenw = chosenw;
      remaining -= chosenw;
      last++;

      printf("element %d: chosenw = %d\n", last - 1, chosenw);

      if (last >= NELEMS(elements))
        break;

      need = PADDING + elements[last].minw;
    }

    i = last; /* 'last' and 'i' are essentially the same variable */

    printf("can place from %d to %d on this line\n", first, last - 1);

    /* place vertical padding */

    if (first > 0) /* don't pad at the top */
    {
      printf("place vertical padding\n");

      err = packer_place_by(packer,
                            loc,
                            MAXWIDTH - 2 * PADDING, PADDING,
                            NULL);
      if (err == result_PACKER_DIDNT_FIT)
      {
        printf("*** could not place vertical padding\n");
        break;
      }
    }

    for (k = first; k < last; k++)
    {
      if (k > first)
      {
        /* place horizontal padding */

        printf("place horizontal padding\n");

        err = packer_place_by(packer,
                              loc,
                              PADDING, elements[k].h,
                              NULL);
        if (err == result_PACKER_DIDNT_FIT)
        {
          printf("*** could not place horizontal padding\n");
          break;
        }
      }

      /* place element */

      printf("place element\n");

      err = packer_place_by(packer,
                            loc,
                            elements[k].chosenw, elements[k].h,
                            &placed);
      if (err == result_PACKER_DIDNT_FIT)
      {
        printf("*** could not place element\n");
        break;
      }
      else if (placed)
      {
        list[j++] = *placed;
      }
    }

    if (remaining < need)
    {
      /* there's space, but it's not enough for the next element - start a
       * new line */

      printf("*** won't fit on this line (only %d left, but need %d)\n",
             remaining, need);

      err = packer_clear(packer, clear);
      if (err)
        goto failure;
    }
  }
  while (i < NELEMS(elements));

  dumppacker(packer, scr);

  dumpboxlist(list, j, scr);

  packer_destroy(packer);

  txtscr_destroy(scr);

  return result_OK;


failure:

  return err;
}

static int test2(void)
{
  static const struct
  {
    packer_loc_t      loc;
    packer_cleardir_t clear;
  }
  tests[] =
  {
    { packer_LOC_TOP_LEFT,  packer_CLEAR_LEFT  },
    { packer_LOC_TOP_RIGHT, packer_CLEAR_RIGHT },
  };

  result_t err;
  int   i;

  for (i = 0; i < NELEMS(tests); i++)
  {
    err = subtest2(tests[i].loc, tests[i].clear);
    if (err)
      goto failure;
  }

  return 0;


failure:

  return 1;
}

int packer_test(void)
{
  result_t err;

  err = test1();
  if (err)
    goto failure;

  err = test2();
  if (err)
    goto failure;

  return result_TEST_PASSED;


failure:

  return result_TEST_FAILED;
}
