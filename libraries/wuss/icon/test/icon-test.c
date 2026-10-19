/* icon/test/icon-test.c -- wuss icon tests */

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

result_t wuss_icon_test(const char *resources)
{
  result_t       rc;
  wuss_fixture_t fixture;
  wuss_t        *wuss;

  rc = wuss_fixture_open(&fixture, 200, 200);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  printf("test: writable icons: click for caret, edit keys, full buffer, Tab, caret loss\n");

  {
    static test_task_t tc_w;
    wuss_task_t       *delegate_w;
    box_t              box_w, content_w;
    wuss_window_t     *win_w;
    wuss_icon_spec_t   specs[3];
    wuss_icon_t       *icons[3];
    int                claimed;

    delegate_w = mk_task(wuss, test_handle, &tc_w);
    if (delegate_w == NULL) goto Failure;

    box_w.x0 = 10; box_w.y0 = 30; box_w.x1 = 110; box_w.y1 = 130;
    rc = wuss_window_create(delegate_w, &box_w, "W",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_w);
    if (rc != result_OK) goto Failure;

    /* a zero-size buffer is rejected */
    wuss_icon_spec_writable(&specs[0], (box_t) { 0, 0, 80, 16 }, NULL, 0, 0);
    if (wuss_icon_create(win_w, &specs[0], NULL) != result_WUSS_BAD_ICON)
      goto Failure;

    wuss_icon_spec_writable(&specs[0], (box_t) { 0, 0, 80, 16 }, "toolong", 4, 0);
    wuss_icon_spec_writable(&specs[1], (box_t) { 0, 20, 80, 36 }, NULL, 16, 0);
    wuss_icon_spec_writable(&specs[2], (box_t) { 0, 40, 80, 56 }, "x", 8,
                            wuss_ICON_FLAGS_HIDDEN);
    rc = wuss_icon_create_array(win_w, specs, 3, icons);
    if (rc != result_OK) goto Failure;

    /* the seed text is truncated to fit */
    if (strcmp(wuss_icon_get_text(icons[0]), "too") != 0) goto Failure;

    /* a click takes the focus and the caret (no font: caret at the end) */
    wuss_window_get_content_bounds(win_w, &content_w);
    wuss_mouse_click(wuss, POINT(content_w.x0 + 5, content_w.y0 + 25),
                     wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, POINT(content_w.x0 + 5, content_w.y0 + 25),
                     wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (wuss_get_focus(wuss) != win_w || wuss->caret.icon != icons[1] ||
        wuss->caret.index != 0)
      goto Failure;

    /* typing inserts at the caret and raises an icon event per edit */
    tc_w.icon_count = 0;
    wuss_key(wuss, 'a', wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, 'c', wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, 'b', wuss_KEY_MOD_NONE, &claimed);
    if (!claimed || strcmp(wuss_icon_get_text(icons[1]), "abc") != 0 ||
        wuss->caret.index != 2 || tc_w.icon_count != 3 ||
        tc_w.key_count != 0)
      goto Failure;

    /* Home, Delete, End, Backspace */
    wuss_key(wuss, wuss_KEY_HOME, wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, wuss_KEY_DELETE, wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, wuss_KEY_END, wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, 8, wuss_KEY_MOD_NONE, &claimed);
    if (strcmp(wuss_icon_get_text(icons[1]), "b") != 0 ||
        wuss->caret.index != 1 || tc_w.icon_count != 5)
      goto Failure;

    /* keys the field doesn't use reach the task */
    wuss_key(wuss, 13, wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, 'w', wuss_KEY_MOD_CTRL, &claimed);
    wuss_key(wuss, wuss_KEY_F1, wuss_KEY_MOD_NONE, &claimed);
    if (tc_w.key_count != 3 || strcmp(wuss_icon_get_text(icons[1]), "b") != 0)
      goto Failure;

    /* non-ASCII keys insert as UTF-8; the caret, Backspace and Delete step
     * over whole codepoints */
    tc_w.icon_count = 0;
    wuss_key(wuss, 0xA9, wuss_KEY_MOD_NONE, &claimed);    /* copyright */
    wuss_key(wuss, 0x263A, wuss_KEY_MOD_NONE, &claimed);  /* smiley */
    wuss_key(wuss, 0x1F600, wuss_KEY_MOD_NONE, &claimed); /* emoji */
    if (!claimed ||
        strcmp(wuss_icon_get_text(icons[1]),
               "b\xC2\xA9\xE2\x98\xBA\xF0\x9F\x98\x80") != 0 ||
        wuss->caret.index != 10 || tc_w.icon_count != 3)
      goto Failure;
    wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_NONE, &claimed);
    if (wuss->caret.index != 6) goto Failure;
    wuss_key(wuss, 8, wuss_KEY_MOD_NONE, &claimed);
    if (wuss->caret.index != 3 ||
        strcmp(wuss_icon_get_text(icons[1]), "b\xC2\xA9\xF0\x9F\x98\x80") != 0)
      goto Failure;
    wuss_key(wuss, wuss_KEY_DELETE, wuss_KEY_MOD_NONE, &claimed);
    wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_NONE, &claimed);
    if (wuss->caret.index != 1 ||
        strcmp(wuss_icon_get_text(icons[1]), "b\xC2\xA9") != 0)
      goto Failure;
    wuss_key(wuss, wuss_KEY_RIGHT, wuss_KEY_MOD_NONE, &claimed);
    if (wuss->caret.index != 3) goto Failure;

    /* a caret placed inside a codepoint snaps to its start */
    wuss_icon_set_caret(win_w, icons[1], 2);
    if (wuss->caret.index != 1) goto Failure;

    /* C1 controls aren't inserted: they reach the task */
    wuss_key(wuss, 0x85, wuss_KEY_MOD_NONE, &claimed);
    if (tc_w.key_count != 4 ||
        strcmp(wuss_icon_get_text(icons[1]), "b\xC2\xA9") != 0)
      goto Failure;

    /* Ctrl+Left/Right jump to the ends; Shift+Left/Right move by words */
    wuss_icon_set_text(win_w, icons[1], "ab cd  ef");
    wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_CTRL, &claimed);
    if (!claimed || wuss->caret.index != 0) goto Failure;
    wuss_key(wuss, wuss_KEY_RIGHT, wuss_KEY_MOD_SHIFT, &claimed);
    if (wuss->caret.index != 2) goto Failure;
    wuss_key(wuss, wuss_KEY_RIGHT, wuss_KEY_MOD_SHIFT, &claimed);
    if (wuss->caret.index != 5) goto Failure;
    wuss_key(wuss, wuss_KEY_RIGHT, wuss_KEY_MOD_SHIFT, &claimed);
    if (wuss->caret.index != 9) goto Failure;
    wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_SHIFT, &claimed);
    if (wuss->caret.index != 7) goto Failure;
    wuss_key(wuss, wuss_KEY_LEFT, wuss_KEY_MOD_SHIFT, &claimed);
    if (wuss->caret.index != 3) goto Failure;
    wuss_key(wuss, wuss_KEY_RIGHT, wuss_KEY_MOD_CTRL, &claimed);
    if (wuss->caret.index != 9) goto Failure;

    /* Ctrl+U clears the field and raises an icon event */
    tc_w.icon_count = 0;
    wuss_key(wuss, 'u', wuss_KEY_MOD_CTRL, &claimed);
    if (!claimed || wuss_icon_get_text(icons[1])[0] != '\0' ||
        wuss->caret.index != 0 || tc_w.icon_count != 1 ||
        tc_w.key_count != 4)
      goto Failure;

    /* clipboard and selection: Ctrl-A selects all, C copies, X cuts, V
     * pastes over the selection, typing replaces it, and field-owned chords
     * never reach the task */
    rc = wuss_icon_set_text(win_w, icons[1], "hello");
    if (rc != result_OK) goto Failure;
    tc_w.key_count = 0;
    wuss_key(wuss, 'a', wuss_KEY_MOD_CTRL, &claimed);
    wuss_key(wuss, 'c', wuss_KEY_MOD_CTRL, &claimed);
    if (strcmp(wuss_clipboard_get(wuss), "hello") != 0 ||
        strcmp(wuss_icon_get_text(icons[1]), "hello") != 0)
      goto Failure;
    wuss_key(wuss, 'x', wuss_KEY_MOD_CTRL, &claimed);
    if (wuss_icon_get_text(icons[1])[0] != '\0' ||
        strcmp(wuss_clipboard_get(wuss), "hello") != 0)
      goto Failure;
    wuss_key(wuss, 'v', wuss_KEY_MOD_CTRL, &claimed);
    if (strcmp(wuss_icon_get_text(icons[1]), "hello") != 0 ||
        wuss->caret.index != 5)
      goto Failure;
    wuss_key(wuss, 'a', wuss_KEY_MOD_CTRL, &claimed);
    wuss_key(wuss, 'y', wuss_KEY_MOD_NONE, &claimed);
    if (strcmp(wuss_icon_get_text(icons[1]), "y") != 0) goto Failure;
    (void) wuss_clipboard_set(wuss, "a\nb");
    wuss_key(wuss, wuss_KEY_INSERT, wuss_KEY_MOD_NONE, &claimed);
    if (strcmp(wuss_icon_get_text(icons[1]), "yab") != 0) goto Failure;
    wuss_key(wuss, 'a', wuss_KEY_MOD_CTRL, &claimed);
    wuss_key(wuss, 8, wuss_KEY_MOD_NONE, &claimed);
    if (wuss_icon_get_text(icons[1])[0] != '\0') goto Failure;
    wuss_key(wuss, 'd', wuss_KEY_MOD_CTRL, &claimed);
    if (!claimed || tc_w.key_count != 0) goto Failure;
    wuss_key(wuss, 9, wuss_KEY_MOD_SHIFT, &claimed);
    wuss_key(wuss, 9, wuss_KEY_MOD_NONE, &claimed);

    /* Tab wraps over the hidden field; Shift-Tab goes back */
    wuss_key(wuss, 9, wuss_KEY_MOD_NONE, &claimed);
    if (!claimed || wuss->caret.icon != icons[0] || wuss->caret.index != 3)
      goto Failure;
    wuss_key(wuss, 9, wuss_KEY_MOD_SHIFT, &claimed);
    if (wuss->caret.icon != icons[1]) goto Failure;

    /* a full buffer swallows further characters */
    wuss_icon_set_caret(win_w, icons[0], -1);
    tc_w.icon_count = 0;
    wuss_key(wuss, 'q', wuss_KEY_MOD_NONE, &claimed);
    if (!claimed || strcmp(wuss_icon_get_text(icons[0]), "too") != 0 ||
        tc_w.icon_count != 0)
      goto Failure;

    /* set_text truncates and moves the caret to the end */
    rc = wuss_icon_set_text(win_w, icons[0], "hi");
    if (rc != result_OK || strcmp(wuss_icon_get_text(icons[0]), "hi") != 0 ||
        wuss->caret.index != 2)
      goto Failure;

    /* ...without splitting a codepoint */
    rc = wuss_icon_set_text(win_w, icons[0], "ab\xC2\xA9");
    if (rc != result_OK || strcmp(wuss_icon_get_text(icons[0]), "ab") != 0)
      goto Failure;

    /* a hidden field can't take the caret; hiding the caret field drops it */
    if (wuss_icon_set_caret(win_w, icons[2], 0) != result_BAD_ARG)
      goto Failure;
    wuss_icon_set_hidden(win_w, icons[0], 1);
    if (wuss->caret.icon != NULL) goto Failure;

    /* losing the focus drops the caret; deleting the caret icon clears it */
    wuss_icon_set_caret(win_w, icons[1], 0);
    if (wuss_set_focus(wuss, NULL) != result_OK || wuss->caret.icon != NULL)
      goto Failure;
    wuss_icon_set_caret(win_w, icons[1], 0);
    wuss_icon_delete(win_w, icons[1]);
    if (wuss->caret.icon != NULL) goto Failure;

    wuss_icon_set_caret(win_w, icons[2], 0); /* hidden: refused */
    wuss_window_close(win_w);
    if (wuss->caret.window != NULL) goto Failure;
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
  }

  printf("test: creating icons on a hidden window never dirties the visible window sitting under its footprint\n");

  {
    static test_task_t tc_i;
    wuss_task_t       *delegate_i;
    box_t              box_visible2, box_hidden2, region;
    wuss_window_t     *win_visible2, *win_hidden_i;
    wuss_icon_t       *icon;
    wuss_icon_spec_t   spec;
    int                i, overlap_x, overlap_y, overlap_dirty;

    tc_i.redraw_count = 0;
    tc_i.mouse_count  = 0;
    delegate_i = mk_task(wuss, test_handle, &tc_i);
    if (delegate_i == NULL) goto Failure;

    /* same overlap shape as the close test above: a visible window at the
     * screen's top-left, and a hidden dialogue-style window over the same
     * area (e.g. wuss_proginfo_window, always built at content (0,0)) */
    box_visible2.x0 = 0; box_visible2.y0 = 0;
    box_visible2.x1 = 80; box_visible2.y1 = 80;
    rc = wuss_window_create(delegate_i, &box_visible2, "V2", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_visible2);
    if (rc != result_OK)
      goto Failure;

    box_hidden2.x0 = 0; box_hidden2.y0 = 0;
    box_hidden2.x1 = 40; box_hidden2.y1 = 40;
    rc = wuss_window_create(delegate_i, &box_hidden2, "Hidden2",
                            wuss_WINDOW_HIDDEN, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_hidden_i);
    if (rc != result_OK)
      goto Failure;

    rc = wuss_redraw_dirty(wuss); /* flush both creates' own invalidates */
    if (rc != result_OK)
      goto Failure;

    /* an icon near the hidden window's own top-left, in window-local
     * (pre-scroll) coordinates -- wuss_info_create builds proginfo's rows
     * this way, each icon punching its own bbox-shaped invalidate */
    memset(&spec, 0, sizeof(spec));
    spec.bbox = (box_t) BOX_POS_SIZE(2, 2, 16, 16);
    spec.type = wuss_ICON_TYPE_LABEL;
    spec.text = "x";
    rc = wuss_icon_create(win_hidden_i, &spec, &icon);
    if (rc != result_OK)
      goto Failure;

    /* content sits inset from the hidden window's own content-space origin
     * by its outline + titlebar (default furniture: 1px outline, 20px
     * titlebar -- same constants the other tests in this file use) */
    overlap_x = box_hidden2.x0 + 1 + 2 + 8; /* centre of the icon's bbox, in screen space */
    overlap_y = box_hidden2.y0 + 1 + 20 + 2 + 8;

    overlap_dirty = 0;
    for (i = 0; i < wuss_get_dirty_count(wuss); i++)
    {
      wuss_get_dirty(wuss, i, &region);
      if (box_contains_point(&region, overlap_x, overlap_y))
        overlap_dirty = 1;
    }
    if (overlap_dirty)
      goto Failure; /* an icon created on a hidden window must not force a
                      * repaint of whatever visible window/backdrop actually
                      * occupies that screen area */

    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK)
      goto Failure;

    wuss_window_close(win_hidden_i);
    wuss_window_close(win_visible2);
  }

  printf("test: wuss__slider_value_to_px / wuss__slider_px_to_value convert "
        "and round-trip\n");

  {
    box_t   groove;
    point_t pt;

    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 100; groove.y1 = 20;

    /* endpoints and midpoint of a [0,100] range over a 100px groove: exact,
     * no rounding needed */
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 0, 0, 100) != 0)
      goto Failure;
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 100, 0, 100) != 100)
      goto Failure;
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 50, 0, 100) != 50)
      goto Failure;

    /* a degenerate [lo,hi] span always reads back as pixel 0 */
    if (wuss__slider_value_to_px(&groove, wuss_SLIDER_HORIZONTAL,
                                 7, 7, 7) != 0)
      goto Failure;

    /* px_to_value rounds to the nearest value rather than truncating: a 10px
     * groove over [0,3] places value 1 at 3.33px, so pixel 3 (0.9 of the way
     * to value 1) must round up to 1, not truncate down to 0 */
    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 10; groove.y1 = 20;

    pt.x = 3; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 3) != 1)
      goto Failure;

    /* pixel 1 (0.3 of the way to value 1) is closer to 0, and must round
     * down rather than up */
    pt.x = 1; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 3) != 0)
      goto Failure;

    /* an exact half-way pixel rounds up to the higher value: a 4px groove
     * over [0,1] puts the 0/1 boundary at pixel 2 */
    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 4; groove.y1 = 20;

    pt.x = 2; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 1) != 1)
      goto Failure;

    /* a point left of the groove clamps to lo, one past its right edge
     * clamps to hi */
    pt.x = -50; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 1) != 0)
      goto Failure;
    pt.x = 50; pt.y = 10;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_HORIZONTAL,
                                 pt, 0, 1) != 1)
      goto Failure;

    /* VERTICAL: pixel 0 is the groove's bottom (y1), value grows upward */
    groove.x0 = 0; groove.y0 = 0;
    groove.x1 = 20; groove.y1 = 10;

    pt.x = 10; pt.y = groove.y1;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_VERTICAL,
                                 pt, 0, 100) != 0)
      goto Failure;
    pt.x = 10; pt.y = groove.y0;
    if (wuss__slider_px_to_value(&groove, wuss_SLIDER_VERTICAL,
                                 pt, 0, 100) != 100)
      goto Failure;
  }

  printf("test: wuss__slider_value_for_point maps a screen click through a "
        "real slider icon, honouring the groove gap and a reversed "
        "min > max\n");

  {
    static test_task_t tc_sl;
    wuss_task_t       *delegate_sl;
    wuss_window_t     *win_sl;
    wuss_icon_t       *icon_sl;
    wuss_icon_spec_t   spec_sl;
    box_t              box_sl, content_sl, screen_box_sl, groove_sl;
    point_t            scroll_sl, pt_sl;

    delegate_sl = mk_task(wuss, test_handle, &tc_sl);
    if (delegate_sl == NULL) goto Failure;

    box_sl.x0 = 5; box_sl.y0 = 5; box_sl.x1 = 125; box_sl.y1 = 105;
    rc = wuss_window_create(delegate_sl, &box_sl, "SL", wuss_WINDOW_DEFAULT,
                            wuss_NO_BACKDROP,
                            SIZE2D(400, 400), SIZE2D(0, 0), &win_sl);
    if (rc != result_OK)
      goto Failure;

    memset(&spec_sl, 0, sizeof(spec_sl));
    spec_sl.bbox                  = (box_t) BOX_POS_SIZE(0, 0, 108, 20);
    spec_sl.type                  = wuss_ICON_TYPE_SLIDER;
    spec_sl.u.slider.orientation  = wuss_SLIDER_HORIZONTAL;
    spec_sl.u.slider.min          = 0;
    spec_sl.u.slider.max          = 100;
    spec_sl.u.slider.default_value = 0;
    rc = wuss_icon_create(win_sl, &spec_sl, &icon_sl);
    if (rc != result_OK)
      goto Failure;

    wuss_window_get_content_bounds(win_sl, &content_sl);
    wuss_window_get_scroll(win_sl, &scroll_sl);
    wuss__icon_box_to_screen(&content_sl, scroll_sl, &spec_sl.bbox,
                             &screen_box_sl);
    wuss__slider_groove_box(&screen_box_sl, &groove_sl);

    /* a click at the groove's on-screen left/right edges reads back as the
     * slider's min/max, confirming wuss__slider_value_for_point applies the
     * WUSS_SLIDER_GAP inset rather than reading off the full icon bbox */
    pt_sl.x = groove_sl.x0; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 0)
      goto Failure;

    pt_sl.x = groove_sl.x1; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 100)
      goto Failure;

    /* a reversed min > max mirrors the reading: the groove's left pixel
     * (nominally "pixel 0") now yields the high end of [lo,hi] */
    wuss_icon_set_value(win_sl, icon_sl, 100);
    icon_sl->spec.u.slider.min = 100;
    icon_sl->spec.u.slider.max = 0;

    pt_sl.x = groove_sl.x0; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 100)
      goto Failure;

    pt_sl.x = groove_sl.x1; pt_sl.y = (groove_sl.y0 + groove_sl.y1) / 2;
    if (wuss__slider_value_for_point(win_sl, icon_sl, pt_sl) != 0)
      goto Failure;

    /* a wheel notch over the groove bumps the value by step towards max
     * (here downwards, as min > max) and leaves the window unscrolled */
    icon_sl->spec.u.slider.step = 5;
    pt_sl.x = (groove_sl.x0 + groove_sl.x1) / 2;
    rc = wuss_scroll(wuss, pt_sl, 1, NULL);
    if (rc != result_OK)
      goto Failure;
    if (wuss_icon_get_value(icon_sl) != 95)
      goto Failure;

    /* the decorative surround outside the groove takes the wheel too */
    rc = wuss_scroll(wuss, POINT(screen_box_sl.x0, screen_box_sl.y0), 1,
                     NULL);
    if (rc != result_OK)
      goto Failure;
    if (wuss_icon_get_value(icon_sl) != 90)
      goto Failure;
    wuss_window_get_scroll(win_sl, &pt_sl);
    if (pt_sl.x != scroll_sl.x || pt_sl.y != scroll_sl.y)
      goto Failure;

    wuss_window_close(win_sl);
  }

  printf("test: wuss_icon_set_disabled shades, blocks clicks, drops the caret and invalidates\n");

  {
    static test_task_t tc_dis;
    wuss_task_t       *delegate_dis;
    wuss_window_t     *win_dis;
    box_t              box_dis, content_dis;
    wuss_icon_spec_t   specs_dis[2];
    wuss_icon_t       *icons_dis[2];
    point_t            at_action;

    delegate_dis = mk_task(wuss, test_handle, &tc_dis);
    if (delegate_dis == NULL) goto Failure;

    box_dis.x0 = 10; box_dis.y0 = 30; box_dis.x1 = 110; box_dis.y1 = 130;
    rc = wuss_window_create(delegate_dis, &box_dis, "D",
                            wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                            wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_dis);
    if (rc != result_OK) goto Failure;

    wuss_icon_spec_action(&specs_dis[0], (box_t) { 0, 0, 80, 16 }, "Go", 0);
    wuss_icon_spec_writable(&specs_dis[1], (box_t) { 0, 20, 80, 36 }, NULL, 8, 0);
    rc = wuss_icon_create_array(win_dis, specs_dis, 2, icons_dis);
    if (rc != result_OK) goto Failure;

    wuss_window_get_content_bounds(win_dis, &content_dis);
    at_action = POINT(content_dis.x0 + 5, content_dis.y0 + 5);

    /* an enabled action icon reports a click */
    tc_dis.icon_count = 0;
    wuss_mouse_click(wuss, at_action, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at_action, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (tc_dis.icon_count == 0) goto Failure;

    /* shading sets the flag, invalidates the icon and blocks clicks */
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
    wuss_icon_set_disabled(win_dis, icons_dis[0], 1);
    if (!(icons_dis[0]->spec.flags & wuss_ICON_FLAGS_DISABLED) ||
        wuss_get_dirty_count(wuss) == 0)
      goto Failure;
    tc_dis.icon_count = 0;
    wuss_mouse_click(wuss, at_action, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at_action, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (tc_dis.icon_count != 0) goto Failure;

    /* the shaded icon still draws */
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;

    /* unshading clears the flag and clicks work again */
    wuss_icon_set_disabled(win_dis, icons_dis[0], 0);
    if ((icons_dis[0]->spec.flags & wuss_ICON_FLAGS_DISABLED) ||
        wuss_get_dirty_count(wuss) == 0)
      goto Failure;
    wuss_mouse_click(wuss, at_action, wuss_BUTTON_SELECT, wuss_MOUSE_DOWN, NULL);
    wuss_mouse_click(wuss, at_action, wuss_BUTTON_SELECT, wuss_MOUSE_UP, NULL);
    if (tc_dis.icon_count == 0) goto Failure;

    /* shading the field holding the caret drops the caret */
    rc = wuss_icon_set_caret(win_dis, icons_dis[1], 0);
    if (rc != result_OK || wuss->caret.icon != icons_dis[1]) goto Failure;
    wuss_icon_set_disabled(win_dis, icons_dis[1], 1);
    if (wuss->caret.icon != NULL) goto Failure;

    wuss_window_close(win_dis);
    rc = wuss_redraw_dirty(wuss);
    if (rc != result_OK) goto Failure;
  }

  printf("test: wuss_icon_create rejects type-scoped flags on the wrong icon type\n");

  {
    static test_task_t tc_flag;
    wuss_task_t       *delegate_flag;
    wuss_window_t     *win_flag;
    box_t              box_flag;
    wuss_icon_spec_t   spec_flag;
    wuss_icon_t       *icon_flag;

    tc_flag.redraw_count = 0;
    tc_flag.mouse_count  = 0;
    delegate_flag = mk_task(wuss, test_handle, &tc_flag);
    if (delegate_flag == NULL) goto Failure;

    box_flag.x0 = 10; box_flag.y0 = 10;
    box_flag.x1 = 50; box_flag.y1 = 50;
    rc = wuss_window_create(delegate_flag, &box_flag, "F",
                            wuss_WINDOW_DEFAULT, wuss_NO_BACKDROP,
                            SIZE2D(0, 0), SIZE2D(0, 0), &win_flag);
    if (rc != result_OK)
      goto Failure;

    /* DEFAULT is ACTION-only; setting it on a LABEL must be rejected */
    wuss_icon_spec_label(&spec_flag, (box_t) { 0, 0, 40, 16 }, "x",
                         wuss_ICON_FLAGS_DEFAULT);
    if (wuss_icon_create(win_flag, &spec_flag, &icon_flag) != result_BAD_ARG)
      goto Failure;

    /* the same spec without the stray flag is accepted */
    wuss_icon_spec_label(&spec_flag, (box_t) { 0, 0, 40, 16 }, "x", 0);
    if (wuss_icon_create(win_flag, &spec_flag, &icon_flag) != result_OK)
      goto Failure;

    wuss_window_close(win_flag);
  }

  wuss_fixture_close(&fixture);

  return result_TEST_PASSED;

Failure:
  printf("wuss_icon_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);

  return result_TEST_FAILED;
}

#else

result_t wuss_icon_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
