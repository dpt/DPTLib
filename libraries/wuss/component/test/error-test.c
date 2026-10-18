/* component/test/error-test.c -- wuss_error tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "framebuf/bitmap.h"
#include "framebuf/bmfont.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "io/path.h"
#include "wuss/task.h"
#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/component/error.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_COMPONENTS)

#include "../../core/impl.h"
#include "../../icon.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

#define ERROR_TEST_NONE (-1)
#define ERROR_TEST_W    320
#define ERROR_TEST_H    240

typedef struct error_test_state
{
  wuss_t *wuss;
  int     calls;
  int     last;   /* wuss_error_choice_t, or ERROR_TEST_NONE */
  int     again;  /* report once more from the callback */
}
error_test_state_t;

static void error_test_choice(wuss_error_choice_t choice, void *opaque)
{
  error_test_state_t *state;

  state = opaque;

  state->calls++;
  state->last = (int) choice;

  if (state->again)
  {
    state->again = 0;
    (void) wuss_error_report(state->wuss, NULL, "Again", "Reported from a callback",
                             wuss_ERROR_BUTTONS_OK, NULL, NULL);
  }
}

/* the icon on the window with this label, or NULL */
static wuss_icon_t *error_test_icon(const wuss_window_t *win,
                                    const char          *text)
{
  int i;

  for (i = 0; i < win->nicons; i++)
    if (strcmp(wuss_icon_get_text(win->icons[i]), text) == 0)
      return win->icons[i];

  return NULL;
}

/* the error window: it is centred on the screen, so under its middle */
static wuss_window_t *error_test_window(wuss_t *wuss)
{
  wuss_window_t *hit;

  hit = NULL;
  (void) wuss_mouse_move(wuss, POINT(ERROR_TEST_W / 2, ERROR_TEST_H / 2),
                         &hit);

  return hit;
}

/* Select-click the button labelled text */
static result_t error_test_click(wuss_t        *wuss,
                                 wuss_window_t *win,
                                 const char    *text)
{
  result_t     rc;
  wuss_icon_t *icon;
  box_t        content;
  point_t      p;

  icon = error_test_icon(win, text);
  if (icon == NULL)
    return result_BAD_ARG;

  wuss_window_get_content_bounds(win, &content);
  p = POINT(content.x0 + (icon->spec.bbox.x0 + icon->spec.bbox.x1) / 2,
            content.y0 + (icon->spec.bbox.y0 + icon->spec.bbox.y1) / 2);

  rc = wuss_mouse_click(wuss, p, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
  if (rc != result_OK)
    return rc;

  return wuss_mouse_click(wuss, p, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
}

/* ----------------------------------------------------------------------- */

result_t error_test(const char *resources)
{
  result_t           rc;
  bmfont_t          *font;
  void              *pixels;
  bitmap_t           bm;
  screen_t           scr;
  wuss_font_desc_t   fdesc;
  wuss_t            *wuss;
  wuss_window_t     *win;
  error_test_state_t state;
  box_t              content;
  int                claimed, width;

  state.wuss  = NULL;
  state.calls = 0;
  state.last  = ERROR_TEST_NONE;
  state.again = 0;
  font        = NULL;
  pixels      = NULL;
  wuss        = NULL;

  /* the layout measures text, so the wuss needs a real font */
  forget_test_tasks();
  rc = bmfont_create(pathf("%s/resources/bmfonts/Tiny/Regular.png",
                           resources), &font);
  if (rc != result_OK)
    goto Failure;

  pixels = malloc((size_t) ERROR_TEST_W * 4 * ERROR_TEST_H);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }
  rc = bitmap_init(&bm, SIZE2D(ERROR_TEST_W, ERROR_TEST_H), pixelfmt_bgrx8888,
                   ERROR_TEST_W * 4, NULL, pixels);
  if (rc != result_OK)
    goto Failure;
  screen_for_bitmap(&scr, &bm);

  fdesc.font       = font;
  fdesc.font_class = wuss_FONT_CLASS_NONE;
  fdesc.name       = NULL;
  rc = wuss_create(&scr, &fdesc, 1, NULL, 0, NULL, NULL, resources, &wuss);
  if (rc != result_OK)
    goto Failure;
  state.wuss = wuss;

  printf("test: wuss_error_report rejects NULL arguments\n");
  if (wuss_error_report(NULL, NULL, "T", "m", wuss_ERROR_BUTTONS_OK, NULL,
                        NULL) != result_NULL_ARG ||
      wuss_error_report(wuss, NULL, "T", NULL, wuss_ERROR_BUTTONS_OK, NULL,
                        NULL) != result_NULL_ARG ||
      wuss_error_pending(wuss) != 0 ||
      wuss_error_pending(NULL) != 0)
    goto Failure;

  printf("test: wuss_error OK-only report shows and reports OK\n");
  rc = wuss_error_report(wuss, NULL, "Disc", "Disc is write protected",
                         wuss_ERROR_BUTTONS_OK, error_test_choice, &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL || win->nicons != 3 || wuss_error_pending(wuss) != 1 ||
      error_test_icon(win, "OK") == NULL ||
      strcmp(wuss_window_get_title(win), "Disc") != 0)
    goto Failure;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 1 || state.last != wuss_ERROR_OK ||
      wuss_error_pending(wuss) != 0 || (win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure;

  printf("test: wuss_error OK and Cancel report either\n");
  rc = wuss_error_report(wuss, NULL, NULL, "Continue?",
                         wuss_ERROR_BUTTONS_OK_CANCEL, error_test_choice,
                         &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL || win->nicons != 4 ||
      strcmp(wuss_window_get_title(win), "Error") != 0)
    goto Failure;
  rc = error_test_click(wuss, win, "Cancel");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 2 || state.last != wuss_ERROR_CANCEL)
    goto Failure;

  rc = wuss_error_report(wuss, NULL, NULL, "Continue?",
                         wuss_ERROR_BUTTONS_OK_CANCEL, error_test_choice,
                         &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 3 || state.last != wuss_ERROR_OK)
    goto Failure;

  printf("test: wuss_error RETURN and ESCAPE\n");
  rc = wuss_error_report(wuss, NULL, NULL, "Continue?",
                         wuss_ERROR_BUTTONS_OK_CANCEL, error_test_choice,
                         &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  rc = wuss_set_focus(wuss, win);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_key(wuss, wuss_KEY_ESCAPE, wuss_KEY_MOD_NONE, &claimed);
  if (rc != result_OK || !claimed)
    goto Failure;
  if (state.calls != 4 || state.last != wuss_ERROR_CANCEL)
    goto Failure;

  rc = wuss_error_report(wuss, NULL, NULL, "No way out", wuss_ERROR_BUTTONS_OK,
                         error_test_choice, &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  rc = wuss_set_focus(wuss, win);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_key(wuss, wuss_KEY_ESCAPE, wuss_KEY_MOD_NONE, &claimed);
  if (rc != result_OK || !claimed)
    goto Failure;
  if (state.calls != 5 || state.last != wuss_ERROR_OK) /* only OK to give */
    goto Failure;

  printf("test: wuss_error queues a report made while one is showing\n");
  rc = wuss_error_report(wuss, NULL, "First", "The first", wuss_ERROR_BUTTONS_OK,
                         error_test_choice, &state);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_error_report(wuss, NULL, "Second", "The second",
                         wuss_ERROR_BUTTONS_OK_CANCEL, error_test_choice,
                         &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL || wuss_error_pending(wuss) != 2 ||
      strcmp(wuss_window_get_title(win), "First") != 0 ||
      win->nicons != 3)
    goto Failure;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 6 || wuss_error_pending(wuss) != 1 ||
      (win->flags & wuss_WINDOW_HIDDEN) != 0 ||
      strcmp(wuss_window_get_title(win), "Second") != 0 ||
      win->nicons != 4)
    goto Failure;
  rc = error_test_click(wuss, win, "Cancel");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 7 || state.last != wuss_ERROR_CANCEL ||
      wuss_error_pending(wuss) != 0 || (win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure;

  printf("test: wuss_error callback may report again\n");
  state.again = 1;
  rc = wuss_error_report(wuss, NULL, "One", "One", wuss_ERROR_BUTTONS_OK,
                         error_test_choice, &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 8 || wuss_error_pending(wuss) != 1 ||
      (win->flags & wuss_WINDOW_HIDDEN) != 0 ||
      strcmp(wuss_window_get_title(win), "Again") != 0)
    goto Failure;

  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  rc = error_test_click(wuss, win, "OK"); /* the report left by the callback */
  if (rc != result_OK)
    goto Failure;

  printf("test: wuss_error refuses a close and cancels instead\n");
  rc = wuss_error_report(wuss, NULL, NULL, "Continue?",
                         wuss_ERROR_BUTTONS_OK_CANCEL, error_test_choice,
                         &state);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  if (wuss_task_close_windows(wuss, wuss_window_get_task(win)) == result_OK)
    goto Failure; /* vetoed */
  if (state.calls != 9 || state.last != wuss_ERROR_CANCEL ||
      wuss_error_pending(wuss) != 0 || (win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure; /* the reports below reuse the live window */

  printf("test: wuss_error app icon adds an icon and widens the window\n");
  rc = wuss_error_report(wuss, NULL, NULL, "Plain", wuss_ERROR_BUTTONS_OK,
                         NULL, NULL);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL)
    goto Failure;
  wuss_window_get_content_bounds(win, &content);
  width = content.x1 - content.x0;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  rc = wuss_error_report(wuss, "error", NULL, "Plain", wuss_ERROR_BUTTONS_OK,
                         NULL, NULL);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL || win->nicons != 4)
    goto Failure;
  wuss_window_get_content_bounds(win, &content);
  if (content.x1 - content.x0 <= width)
    goto Failure;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  rc = wuss_error_report(wuss, "no_such_entry", NULL, "Plain",
                         wuss_ERROR_BUTTONS_OK, NULL, NULL);
  if (rc != result_OK)
    goto Failure;
  win = error_test_window(wuss);
  if (win == NULL || win->nicons != 3)
    goto Failure;
  wuss_window_get_content_bounds(win, &content);
  if (content.x1 - content.x0 != width)
    goto Failure;
  rc = error_test_click(wuss, win, "OK");
  if (rc != result_OK)
    goto Failure;
  if (wuss_error_pending(wuss) != 0)
    goto Failure;

  printf("test: wuss_destroy with a report waiting answers nothing\n");
  rc = wuss_error_report(wuss, NULL, NULL, "Waiting", wuss_ERROR_BUTTONS_OK,
                         error_test_choice, &state);
  if (rc != result_OK)
    goto Failure;
  wuss_destroy(wuss);
  wuss = NULL;
  if (state.calls != 9)
    goto Failure;

  goto Cleanup;

Failure:
  printf("error_test: failed (rc=0x%X)\n", rc);
  rc = result_TEST_FAILED;

Cleanup:
  if (wuss != NULL)
    wuss_destroy(wuss);
  free(pixels);
  if (font != NULL)
    bmfont_destroy(font);

  return rc == result_TEST_FAILED ? result_TEST_FAILED : result_TEST_PASSED;
}

#else

result_t error_test(const char *resources)
{
  NOT_USED(resources);

  return result_TEST_PASSED;
}

#endif
