/* component/test/dcs-test.c -- wuss_dcs tests */

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
#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/component/dcs.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_COMPONENTS)

#include "../../core/impl.h"
#include "../../icon.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

#define DCS_TEST_NONE (-1)
#define DCS_TEST_W    320
#define DCS_TEST_H    240

typedef struct dcs_test_state
{
  int calls;
  int last; /* wuss_dcs_choice_t, or DCS_TEST_NONE */
}
dcs_test_state_t;

static void dcs_test_choice(wuss_dcs_choice_t choice, void *opaque)
{
  dcs_test_state_t *state;

  state = opaque;

  state->calls++;
  state->last = (int) choice;
}

/* the icon on the dialogue's window with this label, or NULL */
static wuss_icon_t *dcs_test_icon(const wuss_window_t *win, const char *text)
{
  int i;

  for (i = 0; i < win->nicons; i++)
    if (strcmp(wuss_icon_get_text(win->icons[i]), text) == 0)
      return win->icons[i];

  return NULL;
}

/* the dialogue's window: what is under the pointer it was centred on */
static wuss_window_t *dcs_test_window(wuss_t *wuss)
{
  wuss_window_t *hit;

  hit = NULL;
  (void) wuss_mouse_move(wuss, POINT(160, 120), &hit);

  return hit;
}

static int dcs_test_width(const wuss_window_t *win)
{
  box_t content;

  wuss_window_get_content_bounds(win, &content);

  return content.x1 - content.x0;
}

/* Select-click the button labelled text */
static result_t dcs_test_click(wuss_t        *wuss,
                               wuss_window_t *win,
                               const char    *text)
{
  result_t     rc;
  wuss_icon_t *icon;
  box_t        content;
  point_t      p;

  icon = dcs_test_icon(win, text);
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

result_t dcs_test(const char *resources)
{
  result_t         rc;
  bmfont_t        *font;
  void            *pixels;
  bitmap_t         bm;
  screen_t         scr;
  wuss_font_desc_t fdesc;
  wuss_t          *wuss;
  wuss_dcs_t      *dcs;
  wuss_window_t   *win;
  dcs_test_state_t state;
  int              w, claimed;

  state.calls = 0;
  state.last  = DCS_TEST_NONE;
  dcs         = NULL;
  font        = NULL;
  pixels      = NULL;
  wuss        = NULL;

  /* the layout measures text, so the wuss needs a real font */
  forget_test_tasks();
  rc = bmfont_create(pathf("%s/resources/bmfonts/Tiny/Regular.png",
                           resources), &font);
  if (rc != result_OK)
    goto Failure;

  pixels = malloc((size_t) DCS_TEST_W * 4 * DCS_TEST_H);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }
  rc = bitmap_init(&bm, SIZE2D(DCS_TEST_W, DCS_TEST_H), pixelfmt_bgrx8888,
                   DCS_TEST_W * 4, NULL, pixels);
  if (rc != result_OK)
    goto Failure;
  screen_for_bitmap(&scr, &bm);

  fdesc.font       = font;
  fdesc.font_class = wuss_FONT_CLASS_NONE;
  fdesc.name       = NULL;
  rc = wuss_create(&scr, &fdesc, 1, NULL, 0, NULL, NULL, resources, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: wuss_dcs_create rejects NULL arguments\n");
  if (wuss_dcs_create(NULL, wuss, wuss_DCS_KIND_DCS, "T", "m",
                      dcs_test_choice, &state) != result_NULL_ARG ||
      wuss_dcs_create(&dcs, NULL, wuss_DCS_KIND_DCS, "T", "m",
                      dcs_test_choice, &state) != result_NULL_ARG ||
      wuss_dcs_create(&dcs, wuss, wuss_DCS_KIND_DCS, "T", NULL,
                      dcs_test_choice, &state) != result_NULL_ARG ||
      wuss_dcs_create(&dcs, wuss, wuss_DCS_KIND_DCS, "T", "m",
                      NULL, &state) != result_NULL_ARG ||
      dcs != NULL)
    goto Failure;
  wuss_dcs_destroy(NULL); /* safe */

  printf("test: wuss_dcs buttons report Discard, Cancel and Save\n");
  rc = wuss_dcs_create(&dcs, wuss, wuss_DCS_KIND_DCS, "Quit",
                       "Document has unsaved changes", dcs_test_choice,
                       &state);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_mouse_move(wuss, POINT(160, 120), NULL);
  if (rc != result_OK)
    goto Failure;

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  win = dcs_test_window(wuss);
  if (win == NULL || (win->flags & wuss_WINDOW_HIDDEN) != 0)
    goto Failure;
  if (win->nicons != 4)
    goto Failure; /* message, Discard, Cancel, Save */
  if (state.calls != 0)
    goto Failure; /* opening reports nothing */

  rc = dcs_test_click(wuss, win, "Discard");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 1 || state.last != wuss_DCS_DISCARD)
    goto Failure;
  if ((win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure; /* a choice dismisses it */

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = dcs_test_click(wuss, win, "Cancel");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 2 || state.last != wuss_DCS_CANCEL)
    goto Failure;

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = dcs_test_click(wuss, win, "Save");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 3 || state.last != wuss_DCS_SAVE)
    goto Failure;

  printf("test: wuss_dcs reports Cancel when closed without a choice\n");
  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_mouse_click(wuss, POINT(2, 2), wuss_BUTTON_SELECT,
                        wuss_MOUSE_DOWN, NULL); /* outside the dialogue */
  if (rc != result_OK)
    goto Failure;
  rc = wuss_mouse_click(wuss, POINT(2, 2), wuss_BUTTON_SELECT, wuss_MOUSE_UP,
                        NULL);
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 4 || state.last != wuss_DCS_CANCEL)
    goto Failure;
  if ((win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure;

  printf("test: wuss_dcs reports Cancel once for a reopen without a choice\n");
  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 5 || state.last != wuss_DCS_CANCEL)
    goto Failure; /* the first showing */
  rc = dcs_test_click(wuss, win, "Save");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 6 || state.last != wuss_DCS_SAVE)
    goto Failure; /* the second showing, not cancelled by the first's close */

  printf("test: wuss_dcs Return picks Save and Escape picks Cancel\n");
  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_set_focus(wuss, win);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_key(wuss, wuss_KEY_RETURN, wuss_KEY_MOD_NONE, &claimed);
  if (rc != result_OK || !claimed)
    goto Failure;
  if (state.calls != 7 || state.last != wuss_DCS_SAVE)
    goto Failure;

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_set_focus(wuss, win);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_key(wuss, wuss_KEY_ESCAPE, wuss_KEY_MOD_NONE, &claimed);
  if (rc != result_OK || !claimed)
    goto Failure;
  if (state.calls != 8 || state.last != wuss_DCS_CANCEL)
    goto Failure;

  printf("test: wuss_dcs_set_message resizes the dialogue and keeps it working\n");
  w = dcs_test_width(win);
  rc = wuss_dcs_set_message(dcs, "A rather longer question than the first "
                                 "one, so the dialogue must grow to fit it");
  if (rc != result_OK)
    goto Failure;
  if (dcs_test_width(win) <= w)
    goto Failure;
  w = dcs_test_width(win);
  if (win->nicons != 4)
    goto Failure; /* the old icons were replaced, not added to */
  if (dcs_test_icon(win, "Save") == NULL ||
      dcs_test_icon(win, "Discard") == NULL ||
      dcs_test_icon(win, "Cancel") == NULL)
    goto Failure;

  rc = wuss_dcs_set_message(dcs, "Short");
  if (rc != result_OK)
    goto Failure;
  if (dcs_test_width(win) >= w)
    goto Failure;
  if (wuss_dcs_set_message(dcs, NULL) != result_NULL_ARG ||
      wuss_dcs_set_message(NULL, "x") != result_NULL_ARG)
    goto Failure;

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = dcs_test_click(wuss, win, "Discard");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 9 || state.last != wuss_DCS_DISCARD)
    goto Failure; /* the rebuilt buttons are wired up */

  printf("test: wuss_dcs_destroy while shown reports nothing\n");
  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  wuss_dcs_destroy(dcs);
  dcs = NULL;
  if (state.calls != 9)
    goto Failure;

  printf("test: wuss_dcs Quit kind has Discard and Cancel only\n");
  state.calls = 0;
  state.last  = DCS_TEST_NONE;
  rc = wuss_dcs_create(&dcs, wuss, wuss_DCS_KIND_QUIT, "Quit",
                       "3 documents have unsaved changes", dcs_test_choice,
                       &state);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_mouse_move(wuss, POINT(160, 120), NULL); /* it opens under here */
  if (rc != result_OK)
    goto Failure;
  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  win = dcs_test_window(wuss);
  if (win == NULL || win->nicons != 3)
    goto Failure; /* message, Discard, Cancel */
  if (dcs_test_icon(win, "Save") != NULL)
    goto Failure;

  rc = dcs_test_click(wuss, win, "Discard");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 1 || state.last != wuss_DCS_DISCARD)
    goto Failure;

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = dcs_test_click(wuss, win, "Cancel");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 2 || state.last != wuss_DCS_CANCEL)
    goto Failure;

  rc = wuss_dcs_set_message(dcs, "1 document has unsaved changes");
  if (rc != result_OK)
    goto Failure;
  if (win->nicons != 3)
    goto Failure; /* still no Save after a relayout */

  rc = wuss_dcs_open(dcs);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_set_focus(wuss, win);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_key(wuss, wuss_KEY_RETURN, wuss_KEY_MOD_NONE, &claimed);
  if (rc != result_OK || !claimed)
    goto Failure;
  if (state.calls != 3 || state.last != wuss_DCS_CANCEL)
    goto Failure; /* Cancel is the Quit dialogue's default */

  wuss_dcs_destroy(dcs);
  dcs = NULL;
  goto Cleanup;

Failure:
  printf("dcs_test: failed (rc=0x%X)\n", rc);
  rc = result_TEST_FAILED;

Cleanup:
  wuss_dcs_destroy(dcs);
  if (wuss != NULL)
    wuss_destroy(wuss);
  free(pixels);
  if (font != NULL)
    bmfont_destroy(font);

  return rc == result_TEST_FAILED ? result_TEST_FAILED : result_TEST_PASSED;
}

#else

result_t dcs_test(const char *resources)
{
  NOT_USED(resources);

  return result_TEST_PASSED;
}

#endif
