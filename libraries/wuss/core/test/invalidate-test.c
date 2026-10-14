/* core/test/invalidate-test.c -- wuss invalidate tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "framebuf/screen.h"
#include "wuss/icon-spec.h"
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

result_t wuss_invalidate_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: invalidating a box already fully covered by an existing "
        "dirty rect keeps the larger rect\n");

  {
    box_t outer, inner, dirty;

    rc = wuss_redraw_dirty(wuss); /* flush anything still pending first */
    if (rc != result_OK)
      goto Failure;

    outer.x0 = 10; outer.y0 = 10;
    outer.x1 = 90; outer.y1 = 90;
    rc = wuss_invalidate(wuss, &outer);
    if (rc != result_OK)
      goto Failure;

    inner.x0 = 30; inner.y0 = 30;
    inner.x1 = 50; inner.y1 = 50; /* fully inside outer: must not shrink it */
    rc = wuss_invalidate(wuss, &inner);
    if (rc != result_OK)
      goto Failure;

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;

    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 != outer.x0 || dirty.y0 != outer.y0 ||
        dirty.x1 != outer.x1 || dirty.y1 != outer.y1)
      goto Failure; /* must still cover the whole outer box, not just inner */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
  }

  printf("test: wuss_window_invalidate clamps a client box to the content "
        "area, never dirtying the furniture around it\n");

  {
    static test_task_t tc_iv;
    wuss_task_t       *delegate_iv;
    wuss_window_t     *win_iv;
    box_t              box_iv, content, local, dirty;
    int                i, w, h, before_a;

    tc_iv.redraw_count = 0;
    tc_iv.mouse_count  = 0;
    delegate_iv = mk_task(wuss, test_handle, &tc_iv);
    if (delegate_iv == NULL) goto Failure;

    box_iv.x0 = 5; box_iv.y0 = 5;
    box_iv.x1 = 125; box_iv.y1 = 85;
    rc = wuss_window_create(delegate_iv, &box_iv, "IV", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_iv);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_iv, &content);

    rc = wuss_redraw_dirty(wuss); /* flush the create's own dirty region first */
    if (rc != result_OK)
      goto Failure;

    /* a box like a ball's swept range at the bottom-right corner, straddling
     * both the content and the furniture (titlebar/outline/scrollbars)
     * around it -- content-local, so translate content's screen origin
     * back off before building it. */
    w = content.x1 - content.x0;
    h = content.y1 - content.y0;
    local.x0 = w - 10;
    local.y0 = h - 10;
    local.x1 = w + 10; /* runs past content.x1 into the furniture */
    local.y1 = h + 10; /* runs past content.y1 into the furniture */
    wuss_window_invalidate(win_iv, &local);

    if (wuss_get_dirty_count(wuss) != 1)
      goto Failure;
    wuss_get_dirty(wuss, 0, &dirty);
    if (dirty.x0 < content.x0 || dirty.y0 < content.y0 ||
        dirty.x1 > content.x1 || dirty.y1 > content.y1)
      goto Failure; /* must be clamped to content, not spill into furniture */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    /* a box entirely outside the content (pure furniture, e.g. a client bug
     * that never clamped at all) must not dirty anything. */
    before_a = tc_iv.redraw_count;
    local.x0 = -50; local.y0 = -50;
    local.x1 = -10; local.y1 = -10;
    wuss_window_invalidate(win_iv, &local);
    if (wuss_get_dirty_count(wuss) != 0)
      goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;
    if (tc_iv.redraw_count != before_a)
      goto Failure;

    /* sweep every furniture-adjacent edge: a content-local box straddling
     * that edge must clamp to it, never crossing into visible's outline. */
    for (i = 0; i < 4; i++)
    {
      switch (i)
      {
      case 0: /* left edge */
        local.x0 = -10; local.y0 = 0;
        local.x1 = 10;  local.y1 = 10;
        break;
      case 1: /* top edge */
        local.x0 = 0;   local.y0 = -10;
        local.x1 = 10;  local.y1 = 10;
        break;
      case 2: /* right edge */
        local.x0 = w - 10; local.y0 = 0;
        local.x1 = w + 10; local.y1 = 10;
        break;
      default: /* bottom edge */
        local.x0 = 0;       local.y0 = h - 10;
        local.x1 = 10;      local.y1 = h + 10;
        break;
      }

      wuss_window_invalidate(win_iv, &local);
      if (wuss_get_dirty_count(wuss) > 0)
      {
        wuss_get_dirty(wuss, 0, &dirty);
        if (dirty.x0 < content.x0 || dirty.y0 < content.y0 ||
            dirty.x1 > content.x1 || dirty.y1 > content.y1)
          goto Failure;
      }
      rc = wuss_redraw_dirty(wuss);
      if (rc != result_OK)
        goto Failure;
    }

    wuss_window_close(win_iv);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_invalidate_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_invalidate_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
