/* icon/test/set-text-test.c -- wuss_icon_set_text tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "framebuf/screen.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/icon-spec.h"
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

/* writable-test.c covers truncation and the caret; these cover the copy,
 * NULL, the repaint and running out of memory. */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

static void *no_malloc(size_t size)
{
  NOT_USED(size);
  return NULL;
}

/* ----------------------------------------------------------------------- */

result_t wuss_set_text_test(const char *resources)
{
  result_t         rc;
  wuss_fixture_t   fixture;
  wuss_t          *wuss;
  test_task_t      tc;
  wuss_task_t     *task;
  box_t            winbox;
  wuss_window_t   *window;
  wuss_icon_spec_t spec;
  wuss_icon_t     *label, *field;
  char             source[8];
  void           *(*real_malloc)(size_t);

  NOT_USED(resources);

  rc = wuss_fixture_open(&fixture, 240, 240);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  memset(&tc, 0, sizeof(tc));
  task = mk_task(wuss, test_handle, &tc);
  if (task == NULL)
    goto Failure;

  winbox.x0 = 10; winbox.y0 = 30; winbox.x1 = 230; winbox.y1 = 230;
  rc = wuss_window_create(task, &winbox, "T", wuss_WINDOW_DEFAULT,
                          wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                          &window);
  if (rc != result_OK)
    goto Failure;

  wuss_icon_spec_label(&spec, (box_t) { 0, 0, 100, 20 }, "old", 0);
  rc = wuss_icon_create(window, &spec, &label);
  if (rc != result_OK)
    goto Failure;
  wuss_icon_spec_writable(&spec, (box_t) { 0, 30, 100, 50 }, "typed", 16, 0);
  rc = wuss_icon_create(window, &spec, &field);
  if (rc != result_OK)
    goto Failure;

  printf("test: set_text copies the text and repaints the icon\n");

  rc = wuss_redraw_dirty(wuss);
  if (rc != result_OK)
    goto Failure;

  strcpy(source, "new");
  rc = wuss_icon_set_text(window, label, source);
  if (rc != result_OK)
    goto Failure;
  source[0] = 'X';
  if (strcmp(wuss_icon_get_text(label), "new") != 0)
    FAIL("label text not copied");
  if (wuss_get_dirty_count(wuss) == 0)
    FAIL("label not repainted");

  printf("test: NULL text is empty\n");

  rc = wuss_icon_set_text(window, label, NULL);
  if (rc != result_OK || strcmp(wuss_icon_get_text(label), "") != 0)
    FAIL("label NULL");

  field->text_scroll = 5;
  rc = wuss_icon_set_text(window, field, NULL);
  if (rc != result_OK || strcmp(wuss_icon_get_text(field), "") != 0)
    FAIL("writable NULL");
  if (field->text_scroll != 0)
    FAIL("writable scroll not reset");

  printf("test: out of memory keeps the old text\n");

  rc = wuss_icon_set_text(window, label, "kept");
  if (rc != result_OK)
    goto Failure;

  real_malloc        = wuss->alloc.malloc;
  wuss->alloc.malloc = no_malloc;
  rc = wuss_icon_set_text(window, label, "lost");
  wuss->alloc.malloc = real_malloc;
  if (rc != result_OOM)
    FAIL("OOM not reported");
  if (strcmp(wuss_icon_get_text(label), "kept") != 0)
    FAIL("OOM lost the old text");

  /* a writable owns a fixed buffer, so needs no memory */
  wuss->alloc.malloc = no_malloc;
  rc = wuss_icon_set_text(window, field, "fits");
  wuss->alloc.malloc = real_malloc;
  if (rc != result_OK || strcmp(wuss_icon_get_text(field), "fits") != 0)
    FAIL("writable under OOM");

  wuss_window_close(window);
  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_set_text_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_set_text_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
