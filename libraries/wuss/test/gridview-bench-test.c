/* wuss/test/gridview-bench-test.c -- time a 100,000 item wuss gridview */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#ifdef WUSS_GADGETS
#include "wuss/gadget/gridview.h"
#endif

#include "test/all-tests.h"

#ifdef WUSS_GADGETS

/* ----------------------------------------------------------------------- */

#define BENCH_COUNT   100000
#define BENCH_LABEL   16     /* bytes per label, including the NUL */
#define BENCH_SCREEN  (SIZE2D(800, 600))
#define BENCH_GLYPH   32     /* glyph is this many pixels square */

typedef struct bench_source
{
  char     (*labels)[BENCH_LABEL];
  bitmap_t  glyph;
  int       item_calls; /* item_fn invocations, to show how often it is hit */
}
bench_source_t;

static void bench_item(int index, void *opaque, wuss_gridview_item_t *out)
{
  bench_source_t *src;

  src = opaque;

  src->item_calls++;
  out->glyph = &src->glyph;
  out->label = src->labels[index];
}

static result_t bench_handle(wuss_window_t      *window,
                             const wuss_event_t *event,
                             void               *task_data)
{
  NOT_USED(window);
  NOT_USED(event);
  NOT_USED(task_data);

  return result_OK;
}

/* Print one line: the whole run in ms, and the cost of one repetition. */
static void bench_report(const char *what, int reps, clock_t start)
{
  double ms;

  ms = (double) (clock() - start) * 1000.0 / (double) CLOCKS_PER_SEC;
  printf("  %-34s %9.3f ms total  %11.3f us each (x%d)\n", what, ms,
         ms * 1000.0 / (double) reps, reps);
}

/* ----------------------------------------------------------------------- */

result_t gridview_bench_test(const char *resources)
{
  result_t         rc;
  bench_source_t   src;
  bmfont_t        *font;
  screen_t         scr;
  bitmap_t         bm;
  void            *pixels;
  wuss_t          *wuss;
  wuss_font_desc_t fdesc;
  wuss_task_desc_t tdesc;
  wuss_task_t     *task;
  wuss_window_t   *window;
  wuss_gridview_t *gv;
  wuss_event_t     ev;
  box_t            content, bounds;
  void            *glyph_pixels;
  clock_t          t;
  int              rowbytes, visible, i, reps;

  printf("test: wuss_gridview with %d items\n", BENCH_COUNT);

  font         = NULL;
  pixels       = NULL;
  glyph_pixels = NULL;
  wuss         = NULL;
  gv           = NULL;
  memset(&src, 0, sizeof(src));

  rc = bmfont_create(pathf("%s/resources/bmfonts/Tiny/Regular.png",
                           resources), &font);
  if (rc != result_OK)
    goto failure;

  rowbytes = BENCH_SCREEN.w * 4;
  pixels   = malloc((size_t) rowbytes * (size_t) BENCH_SCREEN.h);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto failure;
  }

  rc = bitmap_init(&bm, BENCH_SCREEN, pixelfmt_bgrx8888, rowbytes, NULL,
                   pixels);
  if (rc != result_OK)
    goto failure;

  screen_for_bitmap(&scr, &bm);

  src.labels   = malloc((size_t) BENCH_COUNT * BENCH_LABEL);
  glyph_pixels = calloc((size_t) BENCH_GLYPH * BENCH_GLYPH, 4);
  if (src.labels == NULL || glyph_pixels == NULL)
  {
    rc = result_OOM;
    goto failure;
  }

  for (i = 0; i < BENCH_COUNT; i++)
    snprintf(src.labels[i], BENCH_LABEL, "Item %d", i);

  rc = bitmap_init(&src.glyph, SIZE2D(BENCH_GLYPH, BENCH_GLYPH),
                   pixelfmt_bgrx8888, BENCH_GLYPH * 4, NULL, glyph_pixels);
  if (rc != result_OK)
    goto failure;

  fdesc.font       = font;
  fdesc.font_class = wuss_FONT_CLASS_NONE;
  fdesc.name       = NULL;
  rc = wuss_create(&scr, &fdesc, 1, NULL, 0, NULL, NULL, resources, &wuss);
  if (rc != result_OK)
    goto failure;

  tdesc.handle    = bench_handle;
  tdesc.task_data = NULL;
  tdesc.name      = "gridview-bench";
  rc = wuss_task_create(wuss, &tdesc, &task);
  if (rc != result_OK)
    goto failure;

  content = (box_t) BOX_POS_SIZE(0, 0, BENCH_SCREEN.w, BENCH_SCREEN.h);
  rc = wuss_window_create(task, &content, "Bench", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, BENCH_SCREEN, SIZE2D(0, 0),
                          &window);
  if (rc != result_OK)
    goto failure;

  /* ----- create: measures every item once ----- */

  t  = clock();
  rc = wuss_gridview_create(&gv, window, BENCH_COUNT, bench_item, 12, NULL,
                            &src);
  if (rc != result_OK)
    goto failure;
  bench_report("create (measure + reflow)", 1, t);
  printf("    item_fn calls so far: %d (%.2f per item)\n", src.item_calls,
         (double) src.item_calls / BENCH_COUNT);

  printf("    columns %d, cell %dx%d\n", wuss_gridview_get_columns(gv),
         wuss_gridview_get_cell_size(gv).w, wuss_gridview_get_cell_size(gv).h);

  ev.kind = wuss_EVENT_OPEN;
  reps    = 100;
  t       = clock();
  for (i = 0; i < reps; i++)
    (void) wuss_gridview_handle_event(gv, &ev);
  bench_report("OPEN (reflow)", reps, t);

  /* ----- redraw: only the visible rows should be touched ----- */

  wuss_window_get_content_bounds(window, &bounds);
  ev.kind                = wuss_EVENT_REDRAW;
  ev.data.redraw.scr     = &scr;
  ev.data.redraw.content = &bounds;
  ev.data.redraw.bounds  = &bounds;

  reps = 50;
  ev.data.redraw.scroll = POINT(0, 0);
  t                     = clock();
  for (i = 0; i < reps; i++)
    (void) wuss_gridview_handle_event(gv, &ev);
  bench_report("redraw, top of document", reps, t);

  ev.data.redraw.scroll = POINT(0, wuss_gridview_cell_box(gv,
                                                          BENCH_COUNT / 2).y0);
  t = clock();
  for (i = 0; i < reps; i++)
    (void) wuss_gridview_handle_event(gv, &ev);
  bench_report("redraw, middle of document", reps, t);

  ev.data.redraw.scroll = POINT(0, wuss_gridview_cell_box(gv,
                                                          BENCH_COUNT - 1).y0);
  t = clock();
  for (i = 0; i < reps; i++)
    (void) wuss_gridview_handle_event(gv, &ev);
  bench_report("redraw, end of document", reps, t);

  /* ----- selection ----- */

  ev.kind              = wuss_EVENT_MOUSE;
  ev.data.mouse.action = wuss_MOUSE_DOWN;

  /* each SELECT clears the previous selection, which scans every item */
  reps = 1000;
  ev.data.mouse.button = wuss_BUTTON_SELECT;
  t                    = clock();
  for (i = 0; i < reps; i++)
  {
    box_t cell;

    cell                = wuss_gridview_cell_box(gv, i * (BENCH_COUNT / reps));
    ev.data.mouse.point = POINT(cell.x0 + 1, cell.y0 + 1);
    (void) wuss_gridview_handle_event(gv, &ev);
  }
  bench_report("SELECT click (one selected)", reps, t);

  /* select everything, one ADJUST click per item; ADJUST toggles, so start
   * from nothing selected */
  wuss_gridview_clear_selection(gv);
  ev.data.mouse.button = wuss_BUTTON_ADJUST;
  t                    = clock();
  for (i = 0; i < BENCH_COUNT; i++)
  {
    box_t cell;

    cell                = wuss_gridview_cell_box(gv, i);
    ev.data.mouse.point = POINT(cell.x0 + 1, cell.y0 + 1);
    (void) wuss_gridview_handle_event(gv, &ev);
  }
  bench_report("ADJUST click, select all", BENCH_COUNT, t);

  for (i = 0; i < BENCH_COUNT; i++)
  {
    if (!wuss_gridview_is_selected(gv, i))
    {
      printf("  item %d not selected after select all\n", i);
      rc = result_TEST_FAILED;
      goto failure;
    }
  }

  /* toggle just the cells on screen, twice over: each flip dirties a cell
   * that really is inside the window */
  visible = (bounds.y1 - bounds.y0) / wuss_gridview_get_cell_size(gv).h *
            wuss_gridview_get_columns(gv);
  reps    = visible * 2;
  t       = clock();
  for (i = 0; i < reps; i++)
  {
    box_t cell;

    cell                = wuss_gridview_cell_box(gv, i % visible);
    ev.data.mouse.point = POINT(cell.x0 + 1, cell.y0 + 1);
    (void) wuss_gridview_handle_event(gv, &ev);
  }
  bench_report("ADJUST click, on-screen cells", reps, t);

  t = clock();
  wuss_gridview_clear_selection(gv);
  bench_report("clear_selection (all selected)", 1, t);

  t = clock();
  wuss_gridview_clear_selection(gv);
  bench_report("clear_selection (none selected)", 1, t);

  /* ----- whole-grid operations ----- */

  src.item_calls = 0;
  reps           = 20;
  ev.kind        = wuss_EVENT_RELAYOUT;
  t              = clock();
  for (i = 0; i < reps; i++)
    (void) wuss_gridview_handle_event(gv, &ev);
  bench_report("RELAYOUT (remeasure + reflow)", reps, t);
  printf("    item_fn calls per RELAYOUT: %d\n", src.item_calls / reps);

  t = clock();
  for (i = 0; i < reps; i++)
    (void) wuss_gridview_set_count(gv, BENCH_COUNT);
  bench_report("set_count (same count; remeasure + reflow)", reps, t);

  t = clock();
  for (i = 0; i < reps; i++)
    wuss_gridview_set_padding(gv, 2 + i % 4, 2 + i % 4, 2);
  bench_report("set_padding", reps, t);

  wuss_gridview_set_padding(gv, 4, 4, 4);
  reps = 20;
  t    = clock();
  for (i = 0; i < reps; i++)
    wuss_gridview_set_layout(gv, 0, i & 1, 0);
  bench_report("set_layout (alternating pow2)", reps, t);

  t = clock();
  wuss_gridview_destroy(gv);
  gv = NULL;
  bench_report("destroy", 1, t);

  wuss_task_destroy(task);
  wuss_destroy(wuss);
  free(glyph_pixels);
  free(src.labels);
  free(pixels);
  bmfont_destroy(font);

  return result_TEST_PASSED;

failure:
  printf("gridview_bench_test: failed (rc %d)\n", (int) rc);
  return result_TEST_FAILED;
}

#else

result_t gridview_bench_test(const char *resources)
{
  NOT_USED(resources);

  return result_TEST_PASSED;
}

#endif
