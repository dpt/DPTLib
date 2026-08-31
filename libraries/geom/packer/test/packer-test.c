
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "test/txtscr.h"

#include "geom/packer.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

static result_t drawbox(const box_t *box, void *opaque)
{
  txtscr_t *scr = opaque;

  printf("drawbox: <%d,%d-%d,%d>\n", box->x0, box->y0, box->x1, box->y1);

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

    printf("%d placed at: <%d,%d-%d,%d>\n",
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

/* packer_release: a slot handed back becomes available again. */
static int test3(void)
{
  static const box_t pagedims = { 0, 0, 100, 100 };

  packer_t    *packer;
  const box_t *a, *b, *c;
  box_t        freed;
  result_t     err;

  printf("test3: packer_release\n");

  packer = packer_create(&pagedims);
  if (packer == NULL)
    return 1;

  /* fill the page with four 50x50 quads, top-left order */
  err  = packer_place_by(packer, packer_LOC_TOP_LEFT, 50, 50, &a);
  err |= packer_place_by(packer, packer_LOC_TOP_LEFT, 50, 50, &b);
  err |= packer_place_by(packer, packer_LOC_TOP_LEFT, 50, 50, &c);
  err |= packer_place_by(packer, packer_LOC_TOP_LEFT, 50, 50, NULL);
  if (err)
    goto failure;

  /* page is now full: a fifth 50x50 must not fit */
  if (packer_place_by(packer, packer_LOC_TOP_LEFT, 50, 50, NULL)
      != result_PACKER_DIDNT_FIT)
  {
    printf("test3: expected DIDNT_FIT while full\n");
    goto failure;
  }

  /* release the top-left quad, then a 50x50 must fit again, in that slot */
  freed.x0 = 0; freed.y0 = 0; freed.x1 = 50; freed.y1 = 50;
  err = packer_release(packer, &freed);
  if (err)
    goto failure;

  err = packer_place_by(packer, packer_LOC_TOP_LEFT, 50, 50, &a);
  if (err)
  {
    printf("test3: placement after release failed (%d)\n", err);
    goto failure;
  }
  if (a->x0 != 0 || a->y0 != 0 || a->x1 != 50 || a->y1 != 50)
  {
    printf("test3: reused slot <%d,%d-%d,%d>, wanted <0,0-50,50>\n",
           a->x0, a->y0, a->x1, a->y1);
    goto failure;
  }

  /* a box wholly outside the margins is rejected */
  freed.x0 = 200; freed.y0 = 200; freed.x1 = 250; freed.y1 = 250;
  if (packer_release(packer, &freed) != result_PACKER_EMPTY)
  {
    printf("test3: out-of-bounds release not rejected\n");
    goto failure;
  }

  packer_destroy(packer);


  /* gutter: a column just wide enough for one box plus its gutter forces the
   * second placement to stack above the first, gutter between them */
  {
    static const box_t coldims = { 0, 0, 30, 200 };

    box_t first;

    printf("test3: packer_set_gutter\n");

    packer = packer_create(&coldims);
    if (packer == NULL)
      return 1;

    packer_set_gutter(packer, 10);

    /* pos points at the packer's single result buffer, so copy the first
     * placement out before the second overwrites it */
    err = packer_place_by(packer, packer_LOC_BOTTOM_LEFT, 20, 20, &a);
    if (err)
      goto failure;
    first = *a;

    err = packer_place_by(packer, packer_LOC_BOTTOM_LEFT, 20, 20, &b);
    if (err)
      goto failure;

    /* boxes are still 20x20 (the gutter is not added to the result)... */
    if (first.x1 - first.x0 != 20 || first.y1 - first.y0 != 20 ||
        b->x1 - b->x0 != 20 || b->y1 - b->y0 != 20)
    {
      printf("test3: gutter inflated the placed box\n");
      goto failure;
    }
    /* ...but the second sits a full gutter above the first, not flush */
    if (b->y0 - first.y1 != 10)
    {
      printf("test3: gap between placements was %d, wanted 10\n",
             b->y0 - first.y1);
      goto failure;
    }
  }

  packer_destroy(packer);
  return 0;


failure:

  packer_destroy(packer);
  return 1;
}

/* count_and_last: packer_map callback recording the free-area count and the
 * last area visited. */
struct scan
{
  int   count;
  box_t last;
};

static result_t count_and_last(const box_t *area, void *opaque)
{
  struct scan *s = opaque;

  s->count++;
  s->last = *area;

  return result_OK;
}

/* packer_release coalescing: filling a page with mixed sizes then releasing
 * every slot must collapse the free list back to the whole page, so a
 * full-page box fits again. */
static int test4(void)
{
  static const box_t pagedims = { 0, 0, 300, 300 };

  static const int w[6] = { 40, 55, 40, 70, 40, 50 };
  static const int h[6] = { 30, 40, 30, 35, 30, 45 };

  packer_t    *packer;
  const box_t *slot;
  box_t        held[6];
  box_t        full;
  struct scan  s;
  result_t     err;
  int          cycle;
  int          i;

  printf("test4: packer_release coalescing\n");

  packer = packer_create(&pagedims);
  if (packer == NULL)
    return 1;

  packer_set_gutter(packer, 4);

  /* several open/close rounds: the free list must not drift */
  for (cycle = 0; cycle < 4; cycle++)
  {
    for (i = 0; i < 6; i++)
    {
      err = packer_place_by(packer, packer_LOC_BOTTOM_LEFT, w[i], h[i], &slot);
      if (err)
      {
        printf("test4: cycle %d place %d failed (%d)\n", cycle, i, err);
        goto failure;
      }

      /* release what place_by consumed: the box plus its gutter strip */
      held[i]    = *slot;
      held[i].x1 = slot->x1 + 4;
      held[i].y1 = slot->y1 + 4;
    }

    for (i = 5; i >= 0; i--)
    {
      err = packer_release(packer, &held[i]);
      if (err)
      {
        printf("test4: cycle %d release %d failed (%d)\n", cycle, i, err);
        goto failure;
      }
    }

    /* everything is back: exactly one free area, the whole margin */
    s.count = 0;
    err = packer_map(packer, count_and_last, &s);
    if (err)
      goto failure;

    if (s.count != 1)
    {
      printf("test4: cycle %d left %d free areas, wanted 1\n", cycle, s.count);
      goto failure;
    }

    if (s.last.x0 != 0   || s.last.y0 != 0 ||
        s.last.x1 != 300 || s.last.y1 != 300)
    {
      printf("test4: cycle %d free area <%d,%d-%d,%d>, wanted <0,0-300,300>\n",
             cycle, s.last.x0, s.last.y0, s.last.x1, s.last.y1);
      goto failure;
    }
  }

  /* and a box spanning the whole page now fits */
  full.x0 = 0; full.y0 = 0; full.x1 = 300; full.y1 = 300;
  err = packer_place_at(packer, &full);
  if (err)
  {
    printf("test4: full-page placement after reclaim failed (%d)\n", err);
    goto failure;
  }

  packer_destroy(packer);
  return 0;


failure:

  packer_destroy(packer);
  return 1;
}

result_t packer_test(const char *resources)
{
  result_t err;

  NOT_USED(resources);

  err = test1();
  if (err)
    goto failure;

  err = test2();
  if (err)
    goto failure;

  err = test3();
  if (err)
    goto failure;

  err = test4();
  if (err)
    goto failure;

  return result_TEST_PASSED;


failure:

  return result_TEST_FAILED;
}
