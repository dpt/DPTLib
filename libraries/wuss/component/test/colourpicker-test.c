/* component/test/colourpicker-test.c -- wuss_colourpicker tests */

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
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/window.h"
#include "wuss/component/colourpicker.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_COMPONENTS)

#include "../../core/impl.h"
#include "../../icon.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

#define CP_TEST_W 320
#define CP_TEST_H 240

typedef struct cp_test_state
{
  int      calls;
  colour_t last;
}
cp_test_state_t;

static void cp_test_changed(colour_t colour, void *opaque)
{
  cp_test_state_t *state;

  state = opaque;

  state->calls++;
  state->last = colour;
}

/* the first shown icon on the window with this text, or NULL */
static wuss_icon_t *cp_test_icon(const wuss_window_t *win, const char *text)
{
  int i;

  for (i = 0; i < win->nicons; i++)
    if ((win->icons[i]->spec.flags & wuss_ICON_FLAGS_HIDDEN) == 0 &&
        strcmp(wuss_icon_get_text(win->icons[i]), text) == 0)
      return win->icons[i];

  return NULL;
}

/* click the icon with button */
static result_t cp_test_click_icon(wuss_t        *wuss,
                                   wuss_window_t *win,
                                   wuss_icon_t   *icon,
                                   wuss_button_t  button)
{
  result_t rc;
  box_t    content;
  point_t  p;

  if (icon == NULL)
    return result_BAD_ARG;

  wuss_window_get_content_bounds(win, &content);
  p = POINT(content.x0 + (icon->spec.bbox.x0 + icon->spec.bbox.x1) / 2,
            content.y0 + (icon->spec.bbox.y0 + icon->spec.bbox.y1) / 2);

  rc = wuss_mouse_click(wuss, p, button, wuss_MOUSE_DOWN, NULL);
  if (rc != result_OK)
    return rc;

  return wuss_mouse_click(wuss, p, button, wuss_MOUSE_UP, NULL);
}

static result_t cp_test_click(wuss_t        *wuss,
                              wuss_window_t *win,
                              const char    *text,
                              wuss_button_t  button)
{
  return cp_test_click_icon(wuss, win, cp_test_icon(win, text), button);
}

/* replace the hex field's text by typing */
static result_t cp_test_type_hex(wuss_t        *wuss,
                                 wuss_window_t *win,
                                 wuss_icon_t   *hex,
                                 const char    *text)
{
  result_t rc;
  int      claimed;

  rc = cp_test_click_icon(wuss, win, hex, wuss_BUTTON_SELECT);
  if (rc != result_OK)
    return rc;
  rc = wuss_key(wuss, 'U', wuss_KEY_MOD_CTRL, &claimed);
  if (rc != result_OK)
    return rc;
  for (; *text != '\0'; text++)
  {
    rc = wuss_key(wuss, *text, wuss_KEY_MOD_NONE, &claimed);
    if (rc != result_OK)
      return rc;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t colourpicker_test(const char *resources)
{
  result_t             rc;
  bmfont_t            *font;
  void                *pixels;
  bitmap_t             bm;
  screen_t             scr;
  wuss_font_desc_t     fdesc;
  wuss_t              *wuss;
  wuss_colourpicker_t *picker;
  cp_test_state_t      state;
  wuss_window_t       *win;
  wuss_icon_t         *hex;
  colour_t             orange;
  colour_t             blue;
  int                  calls;

  state.calls = 0;
  state.last  = colour_rgb(0, 0, 0);
  picker      = NULL;
  font        = NULL;
  pixels      = NULL;
  wuss        = NULL;
  orange      = colour_rgb(255, 136, 0);
  blue        = colour_rgb(0x12, 0x34, 0x56);

  forget_test_tasks();
  rc = bmfont_create(pathf("%s/resources/bmfonts/Tiny/Regular.png",
                           resources), &font);
  if (rc != result_OK)
    goto Failure;

  pixels = malloc((size_t) CP_TEST_W * 4 * CP_TEST_H);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }
  rc = bitmap_init(&bm, SIZE2D(CP_TEST_W, CP_TEST_H), pixelfmt_bgrx8888,
                   CP_TEST_W * 4, NULL, pixels);
  if (rc != result_OK)
    goto Failure;
  screen_for_bitmap(&scr, &bm);

  fdesc.font       = font;
  fdesc.font_class = wuss_FONT_CLASS_NONE;
  fdesc.name       = NULL;
  rc = wuss_create(&scr, &fdesc, 1, NULL, 0, NULL, NULL, resources, &wuss);
  if (rc != result_OK)
    goto Failure;

  printf("test: wuss_colourpicker_create rejects NULL arguments\n");
  if (wuss_colourpicker_create(NULL, wuss, NULL, cp_test_changed,
                               &state) != result_NULL_ARG ||
      wuss_colourpicker_create(&picker, NULL, NULL, cp_test_changed,
                               &state) != result_NULL_ARG ||
      wuss_colourpicker_create(&picker, wuss, NULL, NULL,
                               &state) != result_NULL_ARG ||
      picker != NULL)
    goto Failure;
  wuss_colourpicker_destroy(NULL); /* safe */

  printf("test: wuss_colourpicker opens on the colour given\n");
  rc = wuss_colourpicker_create(&picker, wuss, NULL, cp_test_changed,
                                &state);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_mouse_move(wuss, POINT(CP_TEST_W / 2, CP_TEST_H / 2), NULL);
  if (rc != result_OK)
    goto Failure;
  rc = wuss_colourpicker_open(picker, blue);
  if (rc != result_OK)
    goto Failure;
  win = NULL;
  (void) wuss_mouse_move(wuss, POINT(CP_TEST_W / 2, CP_TEST_H / 2), &win);
  if (win == NULL || (win->flags & wuss_WINDOW_HIDDEN) != 0)
    goto Failure;
  if (state.calls != 0)
    goto Failure; /* opening reports nothing */
  hex = cp_test_icon(win, "#123456");
  if (hex == NULL)
    goto Failure;
  if (cp_test_icon(win, "Red") == NULL || cp_test_icon(win, "Hue") != NULL)
    goto Failure; /* RGB rows shown, others hidden */
  if (cp_test_icon(win, "52") == NULL)
    goto Failure; /* green slider echoes 0x34 */

  printf("test: wuss_colourpicker takes short hex typed in\n");
  rc = cp_test_type_hex(wuss, win, hex, "#f80");
  if (rc != result_OK)
    goto Failure;
  if (state.calls != 1 || state.last.primary != orange.primary)
    goto Failure; /* only the complete "#f80" reports */
  if (wuss_colourpicker_get_colour(picker).primary != orange.primary)
    goto Failure;
  if (strcmp(wuss_icon_get_text(hex), "#f80") != 0)
    goto Failure; /* the typing is left alone */
  if (cp_test_icon(win, "136") == NULL)
    goto Failure; /* the sliders follow */

  printf("test: wuss_colourpicker model switch keeps the colour\n");
  calls = state.calls;
  rc = cp_test_click(wuss, win, "HSV", wuss_BUTTON_SELECT);
  if (rc != result_OK)
    goto Failure;
  if (cp_test_icon(win, "Hue") == NULL || cp_test_icon(win, "Red") != NULL)
    goto Failure;
  if (cp_test_icon(win, "32") == NULL)
    goto Failure; /* hue of #ff8800 */
  if (state.calls != calls ||
      wuss_colourpicker_get_colour(picker).primary != orange.primary)
    goto Failure;
  rc = cp_test_click(wuss, win, "OKLab", wuss_BUTTON_SELECT);
  if (rc != result_OK)
    goto Failure;
  rc = cp_test_click(wuss, win, "CMYK", wuss_BUTTON_SELECT);
  if (rc != result_OK)
    goto Failure;
  if (cp_test_icon(win, "Black") == NULL)
    goto Failure;
  if (state.calls != calls ||
      wuss_colourpicker_get_colour(picker).primary != orange.primary)
    goto Failure;

  printf("test: wuss_colourpicker Adjust-Cancel restores and stays open\n");
  rc = cp_test_click(wuss, win, "Cancel", wuss_BUTTON_ADJUST);
  if (rc != result_OK)
    goto Failure;
  if (state.last.primary != blue.primary ||
      wuss_colourpicker_get_colour(picker).primary != blue.primary)
    goto Failure;
  if ((win->flags & wuss_WINDOW_HIDDEN) != 0)
    goto Failure;
  if (strcmp(wuss_icon_get_text(hex), "#123456") != 0)
    goto Failure;

  printf("test: wuss_colourpicker Adjust-OK keeps the colour as old\n");
  rc = cp_test_type_hex(wuss, win, hex, "ff8800");
  if (rc != result_OK)
    goto Failure;
  rc = cp_test_click(wuss, win, "OK", wuss_BUTTON_ADJUST);
  if (rc != result_OK)
    goto Failure;
  if ((win->flags & wuss_WINDOW_HIDDEN) != 0)
    goto Failure;
  rc = cp_test_click(wuss, win, "Cancel", wuss_BUTTON_ADJUST);
  if (rc != result_OK)
    goto Failure;
  if (wuss_colourpicker_get_colour(picker).primary != orange.primary)
    goto Failure; /* cancel goes back to the colour OK kept */

  printf("test: wuss_colourpicker Select-OK closes\n");
  rc = cp_test_click(wuss, win, "OK", wuss_BUTTON_SELECT);
  if (rc != result_OK)
    goto Failure;
  if ((win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure;

  printf("test: wuss_colourpicker remembers its model\n");
  rc = wuss_colourpicker_open(picker, blue);
  if (rc != result_OK)
    goto Failure;
  if (cp_test_icon(win, "Black") == NULL)
    goto Failure;

  printf("test: wuss_colourpicker Select-Cancel restores and closes\n");
  rc = cp_test_type_hex(wuss, win, hex, "#000");
  if (rc != result_OK)
    goto Failure;
  rc = cp_test_click(wuss, win, "Cancel", wuss_BUTTON_SELECT);
  if (rc != result_OK)
    goto Failure;
  if (state.last.primary != blue.primary ||
      (win->flags & wuss_WINDOW_HIDDEN) == 0)
    goto Failure;

  wuss_colourpicker_destroy(picker);
  picker = NULL;
  goto Cleanup;

Failure:
  printf("colourpicker_test: failed (rc=0x%X)\n", rc);
  rc = result_TEST_FAILED;

Cleanup:
  wuss_colourpicker_destroy(picker);
  if (wuss != NULL)
    wuss_destroy(wuss);
  free(pixels);
  if (font != NULL)
    bmfont_destroy(font);

  return rc == result_TEST_FAILED ? result_TEST_FAILED : result_TEST_PASSED;
}

#else

result_t colourpicker_test(const char *resources)
{
  NOT_USED(resources);

  return result_TEST_PASSED;
}

#endif
