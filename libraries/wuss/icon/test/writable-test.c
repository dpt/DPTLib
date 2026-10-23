/* icon/test/writable-test.c -- wuss writable icon tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "geom/box.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "io/path.h"
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

/* icon-test.c covers the editing keys on a fontless wuss; these cover what
 * needs a font (scrolling, justification, hit-testing) and the edge cases of
 * the keys and the clipboard. */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

/* Press "code" and return whether the caret field claimed it. */
static int key(wuss_t *wuss, int code, wuss_key_modifiers_t modifiers)
{
  int claimed;

  claimed = 0;
  wuss_key(wuss, code, modifiers, &claimed);

  return claimed;
}

/* Select bytes anchor..index of the caret field, caret at "index". */
static void select_range(wuss_window_t *window,
                         wuss_icon_t   *icon,
                         int            anchor,
                         int            index)
{
  wuss_icon_set_caret(window, icon, index);
  window->wuss->caret.anchor = anchor;
}

static int text_is(const wuss_icon_t *icon, const char *expected)
{
  return strcmp(wuss_icon_get_text(icon), expected) == 0;
}

/* ----------------------------------------------------------------------- */

result_t wuss_writable_test(const char *resources)
{
  result_t           rc;
  wuss_fixture_t     fixture;
  bmfont_t          *font;
  wuss_font_desc_t   desc;
  wuss_t            *wuss;
  static test_task_t tc;
  wuss_task_t       *task;
  box_t              winbox;
  wuss_window_t     *window;
  wuss_icon_spec_t   spec;
  wuss_icon_t       *field;
  int                visible;

  font = NULL;
  rc = wuss_fixture_open(&fixture, 240, 240);
  if (rc != result_OK)
    goto Failure;

  /* the fixture's wuss has no font: swap in one that does */
  rc = bmfont_create(pathf("%s/resources/bmfonts/Tiny/Regular.png", resources),
                     &font);
  if (rc != result_OK)
    goto Failure;
  wuss_destroy(fixture.wuss);
  memset(&desc, 0, sizeof(desc));
  desc.font = font;
  rc = wuss_create(&fixture.scr, &desc, 1, NULL, 0, NULL, NULL, NULL,
                   &fixture.wuss);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  task = mk_task(wuss, test_handle, &tc);
  if (task == NULL)
    goto Failure;

  winbox.x0 = 10; winbox.y0 = 30; winbox.x1 = 230; winbox.y1 = 230;
  rc = wuss_window_create(task, &winbox, "W",
                          wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                          wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                          &window);
  if (rc != result_OK)
    goto Failure;

  /* a narrow field: 32 pixels of text area, far less than the long text */
  wuss_icon_spec_writable(&spec, (box_t) { 0, 0, 40, 16 }, NULL, 32, 0);
  rc = wuss_icon_create(window, &spec, &field);
  if (rc != result_OK)
    goto Failure;
  visible = 40 - 2 * WUSS_WRITABLE_INSET;

  printf("test: the caret stays in view as it moves through long text\n");

  {
    static const char long_text[] = "abcdefghijklmnopqrstuvwxyz";

    int len, i, x, scroll, jumps;

    len = (int) strlen(long_text);
    rc  = wuss_icon_set_text(window, field, long_text);
    if (rc != result_OK) goto Failure;
    rc = wuss_icon_set_caret(window, field, -1);
    if (rc != result_OK) goto Failure;

    jumps = 0;
    if (wuss->caret.index != len || field->text_scroll <= 0)
      FAIL("caret at the end of long text did not scroll");

    /* text wider than the field ignores justification and follows the
     * scroll */
    if (wuss__writable_text_x(wuss, field) !=
        WUSS_WRITABLE_INSET - field->text_scroll)
      FAIL("scrolled text origin");

    /* walk left to the start then right to the end: the caret is always
     * within the visible window, and the field jumps rather than creeps */
    for (i = 0; i < 2 * len; i++)
    {
      scroll = field->text_scroll;
      key(wuss, (i < len) ? wuss_KEY_LEFT : wuss_KEY_RIGHT, wuss_KEY_MOD_NONE);
      x = (int) bmfont_caret_x(font, long_text, wuss->caret.index, NULL);
      if (x < field->text_scroll || x > field->text_scroll + visible)
      {
        printf("  index %d at x=%d, scroll %d\n", wuss->caret.index, x,
               field->text_scroll);
        FAIL("caret out of view");
      }
      if (field->text_scroll != scroll)
        jumps++;
    }
    printf("scroll jumped %d times\n", jumps);
    if (wuss->caret.index != len || jumps < 2)
      FAIL("did not scroll both ways back to the end");

    /* Home resets the scroll outright */
    key(wuss, wuss_KEY_HOME, wuss_KEY_MOD_NONE);
    if (field->text_scroll != 0)
      FAIL("Home left the field scrolled");
  }

  printf("test: short text honours justification and never scrolls\n");

  {
    static const struct
    {
      wuss_icon_flags_t flags;
      const char       *name;
    }
    justs[] =
    {
      { 0,                              "left"   },
      { wuss_ICON_FLAGS_JUSTIFY_CENTRE, "centre" },
      { wuss_ICON_FLAGS_JUSTIFY_RIGHT,  "right"  },
    };

    int i, slack, want;

    rc = wuss_icon_set_text(window, field, "ab");
    if (rc != result_OK) goto Failure;
    slack = visible - (int) bmfont_caret_x(font, "ab", 2, NULL);

    for (i = 0; i < NELEMS(justs); i++)
    {
      printf("sub-test: %s\n", justs[i].name);

      field->spec.flags = (field->spec.flags &
                           ~(wuss_ICON_FLAGS_JUSTIFY_CENTRE |
                             wuss_ICON_FLAGS_JUSTIFY_RIGHT)) | justs[i].flags;
      wuss_icon_set_caret(window, field, -1);

      if (i == 0)
        want = WUSS_WRITABLE_INSET;
      else if (i == 1)
        want = WUSS_WRITABLE_INSET + slack / 2;
      else
        want = WUSS_WRITABLE_INSET + slack;

      if (field->text_scroll != 0 ||
          wuss__writable_text_x(wuss, field) != want)
        FAIL("text origin");
    }
    field->spec.flags &= ~wuss_ICON_FLAGS_JUSTIFY_RIGHT;
  }

  printf("test: a click position maps to the nearest caret index\n");

  {
    int x0, at2;

    rc = wuss_icon_set_text(window, field, "abcd");
    if (rc != result_OK) goto Failure;

    x0  = field->spec.bbox.x0 + wuss__writable_text_x(wuss, field);
    at2 = x0 + (int) bmfont_caret_x(font, "abcd", 2, NULL);
    if (wuss__writable_index_for_x(wuss, field, at2) != 2 ||
        wuss__writable_index_for_x(wuss, field, x0 - 20) != 0 ||
        wuss__writable_index_for_x(wuss, field, x0 + 200) != 4)
      FAIL("index for x");
  }

  printf("test: keys that change nothing raise no event\n");

  {
    char clip[16];

    rc = wuss_icon_set_text(window, field, "");
    if (rc != result_OK) goto Failure;
    wuss_icon_set_caret(window, field, 0);
    tc.icon_count = 0;
    tc.key_count  = 0;

    /* Ctrl-U on an empty field */
    if (!key(wuss, 'u', wuss_KEY_MOD_CTRL))
      FAIL("Ctrl-U not claimed");

    rc = wuss_icon_set_text(window, field, "abc");
    if (rc != result_OK) goto Failure;
    tc.icon_count = 0;

    /* Delete at the end */
    wuss_icon_set_caret(window, field, -1);
    if (!key(wuss, wuss_KEY_DELETE, wuss_KEY_MOD_NONE))
      FAIL("Delete not claimed");

    /* Backspace at the start */
    wuss_icon_set_caret(window, field, 0);
    if (!key(wuss, 8, wuss_KEY_MOD_NONE))
      FAIL("Backspace not claimed");

    /* Ctrl-C and Ctrl-X with no selection leave the clipboard be */
    rc = wuss_clipboard_set(wuss, "keep");
    if (rc != result_OK) goto Failure;
    key(wuss, 'c', wuss_KEY_MOD_CTRL);
    key(wuss, 'x', wuss_KEY_MOD_CTRL);
    strcpy(clip, wuss_clipboard_get(wuss));

    /* pasting an empty clipboard */
    rc = wuss_clipboard_set(wuss, "");
    if (rc != result_OK) goto Failure;
    key(wuss, 'v', wuss_KEY_MOD_CTRL);
    key(wuss, wuss_KEY_INSERT, wuss_KEY_MOD_NONE);

    /* Alt chords pass straight through to the task */
    key(wuss, 'q', wuss_KEY_MOD_ALT);

    if (strcmp(clip, "keep") != 0 || !text_is(field, "abc") ||
        tc.icon_count != 0 || tc.key_count != 1)
      FAIL("no-op keys");
  }

  printf("test: Ctrl-Z drops the selection, leaving the caret\n");

  select_range(window, field, 0, 2);
  if (!key(wuss, 'z', wuss_KEY_MOD_CTRL) ||
      wuss->caret.index != 2 || wuss->caret.anchor != 2 ||
      !text_is(field, "abc"))
    FAIL("Ctrl-Z");

  printf("test: Delete removes a selection\n");

  tc.icon_count = 0;
  select_range(window, field, 1, 2);
  key(wuss, wuss_KEY_DELETE, wuss_KEY_MOD_NONE);
  if (!text_is(field, "ac") || wuss->caret.index != 1 || tc.icon_count != 1)
    FAIL("Delete over a selection");

  printf("test: a full field still loses its selection to a key that won't fit\n");

  {
    wuss_icon_t *small;

    wuss_icon_spec_writable(&spec, (box_t) { 0, 20, 40, 36 }, "abc", 4, 0);
    rc = wuss_icon_create(window, &spec, &small);
    if (rc != result_OK) goto Failure;

    /* "b" out, but a two-byte character doesn't fit in what remains */
    tc.icon_count = 0;
    select_range(window, small, 1, 2);
    key(wuss, 0xA9, wuss_KEY_MOD_NONE);
    if (!text_is(small, "ac") || wuss->caret.index != 1 ||
        wuss->caret.anchor != 1 || tc.icon_count != 1)
      FAIL("full field with selection");

    /* Tab with another live field moves to it */
    key(wuss, 9, wuss_KEY_MOD_NONE);
    if (wuss->caret.icon != field) FAIL("Tab to the other field");

    /* ...and with only one live field stays put, caret at the end */
    wuss_icon_set_disabled(window, small, 1);
    wuss_icon_set_caret(window, field, 0);
    key(wuss, 9, wuss_KEY_MOD_NONE);
    if (wuss->caret.icon != field || wuss->caret.index != 2)
      FAIL("Tab with one field");

    /* a disabled field refuses the caret */
    if (wuss_icon_set_caret(window, small, 0) != result_BAD_ARG)
      FAIL("disabled field took the caret");

    wuss_icon_delete(window, small);
  }

  printf("test: paste replaces a selection, stops when full, fixes bad UTF-8\n");

  rc = wuss_icon_set_text(window, field, "hello");
  if (rc != result_OK) goto Failure;
  rc = wuss_clipboard_set(wuss, "XY");
  if (rc != result_OK) goto Failure;
  select_range(window, field, 1, 4);
  key(wuss, 'v', wuss_KEY_MOD_CTRL);
  if (!text_is(field, "hXYo") || wuss->caret.index != 3)
    FAIL("paste over a selection");

  /* a stray byte pastes as U+FFFD */
  rc = wuss_icon_set_text(window, field, "");
  if (rc != result_OK) goto Failure;
  rc = wuss_clipboard_set(wuss, "a\xFF" "b");
  if (rc != result_OK) goto Failure;
  key(wuss, 'v', wuss_KEY_MOD_CTRL);
  if (!text_is(field, "a\xEF\xBF\xBD" "b"))
    FAIL("paste of bad UTF-8");

  /* the field holds 31 bytes: a longer paste stops there */
  rc = wuss_icon_set_text(window, field, "");
  if (rc != result_OK) goto Failure;
  rc = wuss_clipboard_set(wuss, "0123456789012345678901234567890123456789");
  if (rc != result_OK) goto Failure;
  key(wuss, 'v', wuss_KEY_MOD_CTRL);
  if (!text_is(field, "0123456789012345678901234567890") ||
      wuss->caret.index != 31)
    FAIL("paste past the end of the buffer");

  printf("test: wuss_icon_set_caret: NULL clears, unfocusable windows refuse\n");

  {
    wuss_window_t *other;
    wuss_icon_t   *stuck;

    /* NULL for another window leaves this window's caret alone */
    rc = wuss_window_create(task, &winbox, "N", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                            &other);
    if (rc != result_OK) goto Failure;
    if (wuss_icon_set_caret(other, NULL, 0) != result_OK ||
        wuss->caret.icon != field)
      FAIL("NULL on another window");

    /* a field on a window that can't take the focus can't take the caret */
    wuss_icon_spec_writable(&spec, (box_t) { 0, 0, 40, 16 }, "x", 8, 0);
    rc = wuss_icon_create(other, &spec, &stuck);
    if (rc != result_OK) goto Failure;
    if (wuss_icon_set_caret(other, stuck, 0) != result_BAD_ARG ||
        wuss->caret.icon != field)
      FAIL("unfocusable window took the caret");
    wuss_window_close(other);

    if (wuss_icon_set_caret(window, NULL, 0) != result_OK ||
        wuss->caret.icon != NULL || wuss->caret.window != NULL)
      FAIL("NULL did not clear the caret");

    /* with no caret field, keys go to the task */
    tc.key_count = 0;
    key(wuss, 'a', wuss_KEY_MOD_NONE);
    if (tc.key_count != 1 || !text_is(field, "0123456789012345678901234567890"))
      FAIL("key with no caret");
  }

  wuss_window_close(window);
  wuss_fixture_close(&fixture);
  bmfont_destroy(font);

  return result_TEST_PASSED;

Failure:
  printf("wuss_writable_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);
  if (font != NULL)
    bmfont_destroy(font);

  return result_TEST_FAILED;
}

#else

result_t wuss_writable_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
