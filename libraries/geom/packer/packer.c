/* packer.c -- box packing for layout */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/debug.h"
#include "geom/box.h"
#include "utils/array.h"

#include "geom/packer.h"

#include "impl.h"

#define INITIALAREAS 8

packer_t *packer_create(const box_t *dims)
{
  packer_t *p;

  p = malloc(sizeof(*p));
  if (p == NULL)
    return NULL;

  p->areas = malloc(sizeof(*p->areas) * INITIALAREAS);
  if (p->areas == NULL)
  {
    free(p);
    return NULL;
  }

  p->allocedareas     = INITIALAREAS;

  p->areas[0]         = *dims;
  p->usedareas        = 1;

  p->dims             = *dims;
  p->margins          = *dims;

  p->order            = packer_SORT_TOP_LEFT; /* any will do */
  p->sorted           = 1;

  p->consumed_area.x0 = INT_MAX;
  p->consumed_area.y0 = INT_MAX;
  p->consumed_area.x1 = INT_MIN;
  p->consumed_area.y1 = INT_MIN;

  return p;
}

void packer_destroy(packer_t *doomed)
{
  if (doomed == NULL)
    return;

  free(doomed->areas);
  free(doomed);
}

/* ----------------------------------------------------------------------- */

void packer_set_margins(packer_t *packer, const box_t *margins)
{
  packer->margins.x0 = packer->dims.x0 + margins->x0; /* left */
  packer->margins.y0 = packer->dims.y0 + margins->y0; /* bottom */
  packer->margins.x1 = packer->dims.x1 - margins->x1; /* right */
  packer->margins.y1 = packer->dims.y1 - margins->y1; /* top */

  if (box_is_empty(&packer->margins))
  {
    /* keep valid */

    logf_debug("packer_set_margins: %s", "is empty");

    packer->margins.x1 = packer->margins.x0 + 1;
    packer->margins.y1 = packer->margins.y0 + 1;
  }

  logf_debug("packer_set_margins: margins=<%d,%d-%d,%d>",
              packer->margins.x0,
              packer->margins.y0,
              packer->margins.x1,
              packer->margins.y1);
}

/* ----------------------------------------------------------------------- */

static result_t add_area(packer_t *packer, const box_t *area)
{
  int i;

  logf_debug("add_area: area=<%d,%d-%d,%d>",
             area->x0,
             area->y0,
             area->x1,
             area->y1);

  /* is the new 'area' entirely contained by an existing area? */

  for (i = 0; i < packer->usedareas; i++)
  {
    if (box_is_empty(&packer->areas[i]) ||
        !box_contains_box(area, &packer->areas[i]))
      continue;

    logf_debug("add_area: is contained by: %d <%d,%d-%d,%d>",
               i,
               packer->areas[i].x0,
               packer->areas[i].y0,
               packer->areas[i].x1,
               packer->areas[i].y1);

    return result_OK; /* entirely contained */
  }

#ifdef USE_ARRAY_GROW
  if (array_grow((void **) &packer->areas,
                 sizeof(*packer->areas),
                 packer->usedareas,
                 &packer->allocedareas,
                 1,
                 1))
    return result_OOM;
#else
  if (packer->usedareas + 1 > packer->allocedareas)
  {
    int    n;
    box_t *areas;

    n = packer->allocedareas * 2;

    areas = realloc(packer->areas, n * sizeof(*areas));
    if (areas == NULL)
      return result_OOM;

    packer->areas        = areas;
    packer->allocedareas = n;

    logf_debug("add_area: growing list to %d", n);
  }
#endif

  packer->areas[packer->usedareas++] = *area;

  packer->sorted = 0;

  return result_OK;
}

static result_t remove_area(packer_t *packer, const box_t *area)
{
  result_t err;
  int      n;
  int      i;

  logf_debug("remove_area: <%d,%d-%d,%d>",
             area->x0,
             area->y0,
             area->x1,
             area->y1);

  /* now split up any free areas which overlap with 'area' */

  n = packer->usedareas; /* use a stored copy as we'll be changing
                          * packer->usedareas inside the loop */

  for (i = 0; i < n; i++)
  {
    box_t saved;
    box_t subarea;

    saved = packer->areas[i];

    if (box_is_empty(&saved) || !box_intersects(&saved, area))
      continue;

    /* invalidate the area */

    packer->areas[i].x0 = 0;
    packer->areas[i].y0 = 0;
    packer->areas[i].x1 = 0;
    packer->areas[i].y1 = 0;

    packer->sorted = 0;

    /* store all remaining available areas */

    if (saved.x0 < area->x0)
    {
      /* available space remains on the left */
      subarea    = saved;
      subarea.x1 = area->x0;
      err = add_area(packer, &subarea);
      if (err)
        goto failure;
    }

    if (saved.x1 > area->x1)
    {
      /* available space remains on the right */
      subarea    = saved;
      subarea.x0 = area->x1;
      err = add_area(packer, &subarea);
      if (err)
        goto failure;
    }

    if (saved.y0 < area->y0)
    {
      /* available space remains at the bottom */
      subarea    = saved;
      subarea.y1 = area->y0;
      err = add_area(packer, &subarea);
      if (err)
        goto failure;
    }

    if (saved.y1 > area->y1)
    {
      /* available space remains at the top */
      subarea    = saved;
      subarea.y0 = area->y1;
      err = add_area(packer, &subarea);
      if (err)
        goto failure;
    }
  }

  box_union(&packer->consumed_area, area, &packer->consumed_area);

  return result_OK;


failure:

  return err;
}

/* ----------------------------------------------------------------------- */

#define COMPARE_AREAS(name,ta,tb,tc)                            \
static int compare_areas_##name(const void *va, const void *vb) \
{                                                               \
  const box_t *a = va, *b = vb;                                 \
  int          emptya, emptyb;                                  \
  int          v;                                               \
                                                                \
  emptya = box_is_empty(a);                                     \
  emptyb = box_is_empty(b);                                     \
                                                                \
  /* place invalid boxes towards the end of the list */         \
                                                                \
  if (emptya && emptyb)                                         \
    return 0;                                                   \
  else if (emptya)                                              \
    return 1;                                                   \
  else if (emptyb)                                              \
    return -1;                                                  \
                                                                \
  v = ta; if (v) return v;                                      \
  v = tb; if (v) return v;                                      \
  v = tc;        return v;                                      \
}

COMPARE_AREAS(top_left,     b->y1 - a->y1, a->x0 - b->x0, b->x1 - a->x1)
COMPARE_AREAS(top_right,    b->y1 - a->y1, b->x1 - a->x1, a->x0 - b->x0)
COMPARE_AREAS(bottom_left,  a->y0 - b->y0, a->x0 - b->x0, b->x1 - a->x1)
COMPARE_AREAS(bottom_right, a->y0 - b->y0, b->x1 - a->x1, a->x0 - b->x0)

static void packer_sort(packer_t *packer, packer_sortdir_t order)
{
  box_t       *areas;
  int          usedareas;
  const box_t *end;
  box_t       *b;

  assert(order < packer_SORT__LIMIT);

  if (packer->sorted && packer->order == order)
    return;

  areas     = packer->areas;
  usedareas = packer->usedareas;

  if (usedareas > 1)
  {
    int (*compare)(const void *, const void *);

    logf_debug("packer_sort: sorting areas to %d", order);

    switch (order)
    {
    default:
    case packer_SORT_TOP_LEFT:
      compare = compare_areas_top_left;
      break;
    case packer_SORT_TOP_RIGHT:
      compare = compare_areas_top_right;
      break;
    case packer_SORT_BOTTOM_LEFT:
      compare = compare_areas_bottom_left;
      break;
    case packer_SORT_BOTTOM_RIGHT:
      compare = compare_areas_bottom_right;
      break;
    }

    qsort(areas, usedareas, sizeof(*areas), compare);
  }

  logf_debug("packer_sort: %s", "trimming");

  end = areas + usedareas;

  for (b = areas; b < end; b++)
  {
    int n;

    /* trim away any invalid areas which will have been sorted to the end
     * of the list */

    logf_debug("packer_sort: b=<%d,%d-%d,%d>", b->x0, b->y0, b->x1, b->y1);

    if (!box_is_empty(b))
      continue;

    n = (int)(b - areas);

    logf_debug("packer_sort: trimming to %d long", n);
    packer->usedareas = n;
    break;
  }

  packer->sorted = 1;
  packer->order  = order;
}

/* ----------------------------------------------------------------------- */

static const box_t *packer_next(packer_t *packer)
{
  do
  {
    if (packer->nextindex >= packer->usedareas)
    {
      logf_debug("packer_next: %s", "none left");
      return NULL; /* no more areas */
    }

    box_intersection(&packer->areas[packer->nextindex++],
                     &packer->margins,
                     &packer->nextarea);
  }
  while (box_is_empty(&packer->nextarea));

  logf_debug("packer_next: <%d,%d-%d,%d>",
             packer->nextarea.x0,
             packer->nextarea.y0,
             packer->nextarea.x1,
             packer->nextarea.y1);

  return &packer->nextarea;
}

static const box_t *packer_start(packer_t *packer, packer_sortdir_t order)
{
  packer_sort(packer, order);

  packer->nextindex = 0;

  logf_debug("packer_start: list is %d long", packer->usedareas);

  return packer_next(packer);
}

/* ----------------------------------------------------------------------- */

int packer_next_width(packer_t *packer, packer_loc_t loc)
{
  const box_t *b;

  b = packer_start(packer, (packer_sortdir_t) loc);
  if (!b)
    return 0;

  return b->x1 - b->x0;
}

/* ----------------------------------------------------------------------- */

result_t packer_place_at(packer_t *packer, const box_t *area)
{
  box_t b;

  /* subtract the margins */

  box_intersection(&packer->margins, area, &b);

  if (box_is_empty(&b))
    return result_PACKER_EMPTY;

  return remove_area(packer, &b);
}

result_t packer_place_by(packer_t     *packer,
                         packer_loc_t  loc,
                         int           w,
                         int           h,
                         const box_t **pos)
{
  result_t     err;
  const box_t *b;

  if (pos)
    *pos = NULL;

  if (w == 0 || h == 0)
    return result_PACKER_EMPTY;

  for (b = packer_start(packer, (packer_sortdir_t) loc);
       b;
       b = packer_next(packer))
  {
    if (box_could_hold(b, w, h))
    {
      logf_debug("packer_place_by: %s", "fits");
      break;
    }
  }

  if (!b)
  {
    logf_debug("packer_place_by: %s", "didn't fit");
    return result_PACKER_DIDNT_FIT;
  }

  switch (loc)
  {
  case packer_LOC_TOP_LEFT:
  case packer_LOC_BOTTOM_LEFT:
    packer->placed_area.x0 = b->x0;
    packer->placed_area.x1 = b->x0 + w;
    break;

  case packer_LOC_TOP_RIGHT:
  case packer_LOC_BOTTOM_RIGHT:
    packer->placed_area.x0 = b->x1 - w;
    packer->placed_area.x1 = b->x1;
    break;

  default:
    break;
  }

  switch (loc)
  {
  case packer_LOC_TOP_LEFT:
  case packer_LOC_TOP_RIGHT:
    packer->placed_area.y0 = b->y1 - h;
    packer->placed_area.y1 = b->y1;
    break;

  case packer_LOC_BOTTOM_LEFT:
  case packer_LOC_BOTTOM_RIGHT:
    packer->placed_area.y0 = b->y0;
    packer->placed_area.y1 = b->y0 + h;
    break;

  default:
    break;
  }

  err = remove_area(packer, &packer->placed_area);
  if (err)
    return err;

  if (pos)
    *pos = &packer->placed_area;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t packer_clear(packer_t *packer, packer_cleardir_t clear)
{
  result_t     err;
  int          left, right;
  const box_t *b;
  box_t        clearbox;

  logf_debug("packer_clear: %s", "entered");

  switch (clear)
  {
  default:
  case packer_CLEAR_LEFT:
    left  = packer->margins.x0;
    right = INT_MIN;
    break;
  case packer_CLEAR_RIGHT:
    left  = INT_MAX;
    right = packer->margins.x1;
    break;
  case packer_CLEAR_BOTH:
    left  = packer->margins.x0;
    right = packer->margins.x1;
    break;
  }

  for (b = packer_start(packer, packer_SORT_TOP_LEFT);
       b;
       b = packer_next(packer))
  {
    /* find an area which touches the required edge(s) */

    if (b->x0 <= left && b->x1 >= right)
      break; /* found the edge(s) */
  }

  /* clear from top of found box upwards to top of doc */

  logf_debug("packer_clear: %s", "invalidate area");

  clearbox = packer->margins;

  /* if no edges were found then clear the whole area */

  if (b)
    clearbox.y0 = b->y1;

  err = remove_area(packer, &clearbox);
  if (err)
    return err;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t packer_map(packer_t *packer, packer_map_fn_t *fn, void *opaque)
{
  result_t     err;
  const box_t *b;

  for (b = packer_start(packer, packer->order);
       b;
       b = packer_next(packer))
  {
    err = fn(b, opaque);
    if (err)
      return err;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

const box_t *packer_get_consumed_area(const packer_t *packer)
{
  return &packer->consumed_area;
}
