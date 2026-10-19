/* icon/test/icon-draw-test.c -- wuss icon drawing tests */

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
#include "framebuf/colour.h"
#include "framebuf/screen.h"
#include "io/path.h"
#include "wuss/wuss.h"
#include "wuss/task.h"
#include "wuss/window.h"
#include "wuss/icon-spec.h"
#include "wuss/icon.h"
#include "../../icon.h"

#include "test/all-tests.h"

#if defined(WUSS_FURNITURE) && defined(WUSS_ICONS)

#include "../../core/impl.h"
#include "../../test/harness.h"

/* ----------------------------------------------------------------------- */

/* Everything below draws straight through wuss__icon_draw onto a screen wiped
 * to this colour, which no palette entry uses, then reads pixels back. */
#define WIPE colour_rgb(255, 0, 255)

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

typedef struct draw_env
{
  wuss_fixture_t *fixture;
  wuss_window_t  *window;
  box_t           content;
}
draw_env_t;

static pixelfmt_any_t px(const draw_env_t *env, int x, int y)
{
  const unsigned int *pixels = env->fixture->pixels;

  return pixels[y * env->fixture->bm.size.w + x] & 0x00FFFFFF;
}

static pixelfmt_any_t pixel_of(const draw_env_t *env, colour_t colour)
{
  return screen_colour_to_pixel(&env->fixture->scr, colour) & 0x00FFFFFF;
}

/* Non-zero if the pixel at (x,y) is palette entry "idx". */
static int is(const draw_env_t *env, int x, int y, wuss_colour_t idx)
{
  return px(env, x, y) == pixel_of(env, env->fixture->wuss->palette[idx]);
}

/* Count the pixels of colour "colour" inside "b". */
static int count(const draw_env_t *env, const box_t *b, colour_t colour)
{
  pixelfmt_any_t want;
  int            n, x, y;

  want = pixel_of(env, colour);
  n    = 0;
  for (y = b->y0; y < b->y1; y++)
    for (x = b->x0; x < b->x1; x++)
      if (px(env, x, y) == want)
        n++;

  return n;
}

/* The leftmost column inside "b" holding a pixel of colour "colour", or -1. */
static int leftmost(const draw_env_t *env, const box_t *b, colour_t colour)
{
  pixelfmt_any_t want;
  int            x, y;

  want = pixel_of(env, colour);
  for (x = b->x0; x < b->x1; x++)
    for (y = b->y0; y < b->y1; y++)
      if (px(env, x, y) == want)
        return x;

  return -1;
}

/* Wipe the screen and open its clip to the whole of it: outside a redraw the
 * clip is left empty, which the bitmap blit honours. */
static void wipe(draw_env_t *env)
{
  screen_t *scr = &env->fixture->scr;

  scr->clip = (box_t) { 0, 0, env->fixture->bm.size.w,
                        env->fixture->bm.size.h };
  screen_fill_rect(scr, 0, 0, env->fixture->bm.size, WIPE);
}

/* Create an icon from "spec", wipe the screen, draw the icon and return its
 * screen box in "b". The icon is left on the window. */
static result_t draw(draw_env_t             *env,
                     const wuss_icon_spec_t *spec,
                     wuss_icon_t           **icon,
                     box_t                  *b)
{
  result_t rc;

  rc = wuss_icon_create(env->window, spec, icon);
  if (rc != result_OK)
    return rc;

  wuss__icon_box_to_screen(&env->content, POINT(0, 0), &(*icon)->spec.bbox, b);

  wipe(env);
  wuss__icon_draw(env->fixture->wuss, env->window, *icon, &env->content,
                  POINT(0, 0));

  return result_OK;
}

/* Redraw an already created icon, e.g. after changing its state. */
static void redraw(draw_env_t *env, wuss_icon_t *icon)
{
  wipe(env);
  wuss__icon_draw(env->fixture->wuss, env->window, icon, &env->content,
                  POINT(0, 0));
}

/* ----------------------------------------------------------------------- */

result_t wuss_icon_draw_test(const char *resources)
{
  /* the built-in palette is only black and white: offer enough entries for
   * each role set below to resolve to its own colour */
  static const colour_t palette[] =
  {
    { 0xFF000000 }, { 0xFFFFFFFF }, { 0xFF0000FF }, { 0xFF00FF00 },
    { 0xFFFF0000 }, { 0xFF00FFFF }, { 0xFFFFFF00 }, { 0xFFBBBBBB }
  };

  result_t           rc;
  wuss_fixture_t     fixture;
  bmfont_t          *font;
  wuss_font_desc_t   desc;
  wuss_t            *wuss;
  wuss_config_t      config;
  static test_task_t tc;
  wuss_task_t       *task;
  box_t              winbox;
  draw_env_t         env;
  wuss_icon_spec_t   spec;
  wuss_icon_t       *icon;
  box_t              b;
  colour_t           fg, white, light, dark, divider, accent;

  font = NULL;
  rc = wuss_fixture_open(&fixture, 240, 240);
  if (rc != result_OK)
    goto Failure;

  /* the fixture's wuss has no font: swap in one that does, so text draws */
  rc = bmfont_create(pathf("%s/resources/bmfonts/Tiny/Regular.png", resources),
                     &font);
  if (rc != result_OK)
    goto Failure;
  wuss_destroy(fixture.wuss);
  memset(&desc, 0, sizeof(desc));
  desc.font = font;
  rc = wuss_create(&fixture.scr, &desc, 1, palette, NELEMS(palette), NULL,
                   NULL, NULL, &fixture.wuss);
  if (rc != result_OK)
    goto Failure;
  wuss = fixture.wuss;

  /* with no config every bevel role is palette entry 0: give each role its
   * own colour so the checks below can tell them apart */
  wuss_get_config(wuss, &config);
  config.bevel.light    = wuss_COLOUR_WHITE;
  config.bevel.dark     = wuss_COLOUR_BLUE;
  config.bevel.divider  = wuss_COLOUR_CYAN;
  config.button.bg      = wuss_COLOUR_GREY;
  config.button.fg      = wuss_COLOUR_WHITE;
  config.button.pressed = wuss_COLOUR_YELLOW;
  config.accent.colour  = wuss_COLOUR_GREEN;
  config.slider.track   = wuss_COLOUR_WHITE;
  config.slider.value   = wuss_COLOUR_GREEN;
  rc = wuss_set_config(wuss, &config);
  if (rc != result_OK)
    goto Failure;

  task = mk_task(wuss, test_handle, &tc);
  if (task == NULL)
    goto Failure;

  winbox.x0 = 10; winbox.y0 = 30; winbox.x1 = 230; winbox.y1 = 230;
  rc = wuss_window_create(task, &winbox, "D",
                          wuss_WINDOW_DEFAULT | wuss_WINDOW_FOCUSABLE,
                          wuss_NO_BACKDROP, SIZE2D(0, 0), SIZE2D(0, 0),
                          &env.window);
  if (rc != result_OK)
    goto Failure;

  env.fixture = &fixture;
  wuss_window_get_content_bounds(env.window, &env.content);

  white   = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_WHITE)];
  light   = wuss->palette[wuss->bevel.light];
  dark    = wuss->palette[wuss->bevel.dark];
  divider = wuss->palette[wuss->bevel.divider];
  accent  = wuss->palette[wuss->accent];

  printf("test: a hidden icon draws nothing\n");

  wuss_icon_spec_label(&spec, (box_t) { 0, 0, 60, 16 }, "Hi",
                       wuss_ICON_FLAGS_HIDDEN);
  spec.bg = wuss_COLOUR_WHITE;
  rc = draw(&env, &spec, &icon, &b);
  if (rc != result_OK) goto Failure;
  if (count(&env, &b, WIPE) != (b.x1 - b.x0) * (b.y1 - b.y0))
    FAIL("hidden icon touched the screen");

  printf("test: label borders\n");

  {
    static const struct
    {
      wuss_icon_border_t border;
      const char        *name;
    }
    borders[] =
    {
      { wuss_ICON_BORDER_RIDGE,   "ridge"   },
      { wuss_ICON_BORDER_GROOVE,  "groove"  },
      { wuss_ICON_BORDER_PLAIN,   "plain"   },
      { wuss_ICON_BORDER_ACTION,  "action"  },
      { wuss_ICON_BORDER_DIVIDER, "divider" },
    };

    int i, bw;
    int ok;

    bw = WUSS_BEVEL_WIDTH;

    for (i = 0; i < NELEMS(borders); i++)
    {
      printf("sub-test: %s\n", borders[i].name);

      wuss_icon_spec_label(&spec, (box_t) { 0, 0, 100, 24 }, "Ab", 0);
      spec.u.label.border = borders[i].border;
      rc = draw(&env, &spec, &icon, &b);
      if (rc != result_OK) goto Failure;
      fg = wuss->palette[icon->spec.fg];

      switch (borders[i].border)
      {
      case wuss_ICON_BORDER_RIDGE: /* raised: light top/left */
        ok = px(&env, b.x0, b.y0) == pixel_of(&env, light) &&
             px(&env, b.x1 - 1, b.y1 - 1) == pixel_of(&env, dark);
        break;
      case wuss_ICON_BORDER_GROOVE: /* sunken */
        ok = px(&env, b.x0, b.y0) == pixel_of(&env, dark) &&
             px(&env, b.x1 - 1, b.y1 - 1) == pixel_of(&env, light);
        break;
      case wuss_ICON_BORDER_PLAIN:
        ok = px(&env, b.x0, b.y0) == pixel_of(&env, fg) &&
             px(&env, b.x1 - 1, b.y1 - 1) == pixel_of(&env, fg);
        break;
      case wuss_ICON_BORDER_ACTION: /* sunken outset, accent moat */
        ok = px(&env, b.x0, b.y0) == pixel_of(&env, dark) &&
             px(&env, b.x0 + bw, b.y0 + bw) == pixel_of(&env, accent) &&
             px(&env, b.x0 + 2 * bw, b.y0 + 2 * bw) == pixel_of(&env, light);
        break;
      case wuss_ICON_BORDER_DIVIDER: /* sunken outer, raised inner */
        ok = px(&env, b.x0, b.y0) == pixel_of(&env, divider) &&
             px(&env, b.x0 + bw, b.y0 + bw) == pixel_of(&env, light);
        break;
      default:
        ok = 0;
        break;
      }
      if (!ok)
        FAIL("border colours");

      /* text drawn whatever the border */
      if (count(&env, &b, fg) == 0)
        FAIL("no text");

      wuss_icon_delete(env.window, icon);
    }
  }

  printf("test: label background and justification\n");

  {
    static const int justify[3] =
    {
      wuss_ICON_FLAGS_JUSTIFY_LEFT,
      wuss_ICON_FLAGS_JUSTIFY_CENTRE,
      wuss_ICON_FLAGS_JUSTIFY_RIGHT
    };

    int i;
    int x[3];

    for (i = 0; i < 3; i++)
    {
      wuss_icon_spec_label(&spec, (box_t) { 0, 0, 120, 20 }, "Ab", justify[i]);
      spec.bg = wuss_COLOUR_WHITE;
      rc = draw(&env, &spec, &icon, &b);
      if (rc != result_OK) goto Failure;
      fg = wuss->palette[icon->spec.fg];

      /* the background fills the box: both ends are clear of the text */
      if (px(&env, b.x0, b.y0) != pixel_of(&env, white) ||
          px(&env, b.x1 - 1, b.y1 - 1) != pixel_of(&env, white))
        FAIL("background not filled");

      x[i] = leftmost(&env, &b, fg);
      wuss_icon_delete(env.window, icon);
    }

    printf("text starts at x=%d (left), %d (centre), %d (right)\n",
           x[0], x[1], x[2]);
    if (x[0] < 0 || !(x[0] < x[1] && x[1] < x[2]))
      FAIL("justification");
  }

  printf("test: pattern, enabled and disabled\n");

  {
    colour_t bg;

    memset(&spec, 0, sizeof(spec));
    spec.type            = wuss_ICON_TYPE_PATTERN;
    spec.bbox            = (box_t) { 0, 0, 32, 32 };
    spec.fg              = wuss_COLOUR_BLACK;
    spec.bg              = wuss_COLOUR_WHITE;
    spec.u.pattern.tile  = screen_PATTERN_DIAGONAL;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    fg = wuss->palette[icon->spec.fg];
    bg = wuss->palette[icon->spec.bg];

    if (count(&env, &b, fg) == 0 || count(&env, &b, bg) == 0)
      FAIL("enabled pattern lacks one of its colours");

    /* disabled folds the pattern into its ground */
    icon->spec.flags |= wuss_ICON_FLAGS_DISABLED;
    redraw(&env, icon);
    if (count(&env, &b, bg) != (b.x1 - b.x0) * (b.y1 - b.y0))
      FAIL("disabled pattern not all ground");

    wuss_icon_delete(env.window, icon);
  }

  printf("test: action buttons: plain, pressed, default, disabled\n");

  {
    colour_t face, pressed_face, label_fg;
    int      bw;

    bw           = WUSS_BEVEL_WIDTH;
    pressed_face = wuss->palette[wuss->button.pressed];

    wuss_icon_spec_action(&spec, (box_t) { 0, 0, 80, 24 }, "Go", 0);
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    face     = wuss->palette[icon->spec.bg];
    label_fg = wuss->palette[icon->spec.fg];

    if (px(&env, b.x0, b.y0) != pixel_of(&env, light) ||
        px(&env, b.x1 - 1, b.y1 - 1) != pixel_of(&env, dark) ||
        px(&env, b.x0 + bw, b.y0 + bw) != pixel_of(&env, face) ||
        count(&env, &b, label_fg) == 0)
      FAIL("plain button");

    /* held: bevel flips and the face takes the pressed shade */
    wuss__icon_set_state(icon, wuss_ICON_STATE_PRESSED, 1);
    redraw(&env, icon);
    if (px(&env, b.x0, b.y0) != pixel_of(&env, dark) ||
        px(&env, b.x0 + bw, b.y0 + bw) != pixel_of(&env, pressed_face))
      FAIL("pressed button");
    wuss__icon_set_state(icon, wuss_ICON_STATE_PRESSED, 0);

    /* disabled: the label sinks to bevel dark */
    icon->spec.flags |= wuss_ICON_FLAGS_DISABLED;
    redraw(&env, icon);
    {
      box_t inner;

      inner = box_grown(&b, -bw);
      if (count(&env, &inner, label_fg) != 0 ||
          count(&env, &inner, dark) == 0)
        FAIL("disabled button label");
    }
    wuss_icon_delete(env.window, icon);

    /* default: the action surround round an accent-coloured face */
    wuss_icon_spec_action(&spec, (box_t) { 0, 0, 80, 32 }, "OK", 1);
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    if (px(&env, b.x0, b.y0) != pixel_of(&env, dark) ||
        px(&env, b.x0 + bw, b.y0 + bw) != pixel_of(&env, accent) ||
        count(&env, &b, wuss->palette[wuss->button.fg]) == 0)
      FAIL("default button");

    /* pressed default: only the inset ring flips */
    wuss__icon_set_state(icon, wuss_ICON_STATE_PRESSED, 1);
    redraw(&env, icon);
    if (px(&env, b.x0, b.y0) != pixel_of(&env, dark) ||
        px(&env, b.x0 + 2 * bw, b.y0 + 2 * bw) != pixel_of(&env, dark))
      FAIL("pressed default button");

    wuss_icon_delete(env.window, icon);
  }

  printf("test: frame: divider border below the caption\n");

  wuss_icon_spec_frame(&spec, (box_t) { 0, 0, 120, 60 }, "Group");
  rc = draw(&env, &spec, &icon, &b);
  if (rc != result_OK) goto Failure;
  fg = wuss->palette[icon->spec.fg];
  {
    box_t caption;

    caption    = b;
    caption.y1 = b.y0 + WUSS_FRAME_CAPTION_TOP + 8;
    if (px(&env, b.x0, b.y0 + WUSS_FRAME_CAPTION_TOP) !=
          pixel_of(&env, divider) ||
        px(&env, b.x1 - 1, b.y1 - 1) != pixel_of(&env, light) ||
        count(&env, &caption, fg) == 0)
      FAIL("frame");
  }
  wuss_icon_delete(env.window, icon);

  printf("test: radio and option glyphs, no icon set\n");

  {
    int gh, gy, cx, cy;

    /* radio: a ring, filled when selected */
    wuss_icon_spec_radio(&spec, (box_t) { 0, 0, 100, 20 }, "One", 0);
    spec.bg = wuss_COLOUR_WHITE;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    fg = wuss->palette[icon->spec.fg];

    bmfont_get_info(font, NULL, &gh, NULL, NULL);
    gh = CLAMP(gh, 8, b.y1 - b.y0);
    gy = b.y0 + (b.y1 - b.y0 - gh) / 2;
    cx = b.x0 + gh / 2;
    cy = gy + gh / 2;

    if (px(&env, b.x0 + 2, gy) != pixel_of(&env, fg) ||
        px(&env, cx, cy) != pixel_of(&env, white))
      FAIL("unselected radio");
    wuss__icon_set_state(icon, wuss_ICON_STATE_SELECTED, 1);
    redraw(&env, icon);
    if (px(&env, cx, cy) != pixel_of(&env, fg))
      FAIL("selected radio has no centre");
    wuss_icon_delete(env.window, icon);

    /* option: a box, ticked when selected; right-justified puts the glyph
     * at the right edge */
    wuss_icon_spec_option(&spec, (box_t) { 0, 0, 100, 20 }, "Two");
    spec.bg    = wuss_COLOUR_WHITE;
    spec.flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;

    if (px(&env, b.x1 - 1, cy) != pixel_of(&env, fg) ||
        px(&env, b.x0, cy) == pixel_of(&env, fg) ||
        px(&env, b.x1 - gh + 2, gy + gh / 2) != pixel_of(&env, white))
      FAIL("unselected right-justified option");
    wuss__icon_set_state(icon, wuss_ICON_STATE_SELECTED, 1);
    redraw(&env, icon);
    if (px(&env, b.x1 - gh + 2, gy + gh / 2) != pixel_of(&env, fg))
      FAIL("selected option has no tick");

    /* disabled: glyph and label sink to bevel dark */
    icon->spec.flags |= wuss_ICON_FLAGS_DISABLED;
    redraw(&env, icon);
    if (px(&env, b.x1 - 1, cy) != pixel_of(&env, dark) ||
        count(&env, &b, fg) != 0)
      FAIL("disabled option");
    wuss_icon_delete(env.window, icon);
  }

  printf("test: bitmaps: missing image, bevel, latched\n");

  {
    colour_t red;

    red = colour_rgb(255, 0, 0);

    /* a name the pool lacks: a red placeholder box */
    memset(&spec, 0, sizeof(spec));
    spec.type = wuss_ICON_TYPE_BITMAP;
    spec.u.bitmap.name = "nosuchicon";
    spec.bbox = (box_t) { 0, 0, 24, 24 };
    spec.fg   = wuss_COLOUR_BLACK;
    spec.bg   = wuss_NO_BACKGROUND;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    if (count(&env, &b, red) != (b.x1 - b.x0) * (b.y1 - b.y0))
      FAIL("missing bitmap placeholder");
    wuss_icon_delete(env.window, icon);

    /* an image in a ridge: raised at rest, sunk and filled when latched */
    rc = wuss_iconpool_load_resource(wuss, resources);
    if (rc != result_OK) goto Failure;

    spec.u.bitmap.name   = "radon";
    spec.u.bitmap.border = wuss_ICON_BORDER_RIDGE;
    spec.bbox            = (box_t) { 0, 0, 40, 40 };
    spec.bg              = wuss_COLOUR_WHITE;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    if (px(&env, b.x0, b.y0) != pixel_of(&env, light) ||
        px(&env, b.x1 - 1, b.y1 - 1) != pixel_of(&env, dark) ||
        count(&env, &b, red) != 0)
      FAIL("ridged bitmap at rest");

    wuss__icon_set_state(icon, wuss_ICON_STATE_SELECTED, 1);
    redraw(&env, icon);
    if (px(&env, b.x0, b.y0) != pixel_of(&env, dark) ||
        px(&env, b.x0 + WUSS_BEVEL_WIDTH, b.y0 + WUSS_BEVEL_WIDTH) !=
          pixel_of(&env, white))
      FAIL("latched bitmap");
    wuss_icon_delete(env.window, icon);
  }

  printf("test: radio from the icon set\n");

  {
    const bitmap_t *bm;

    bm = wuss_iconpool_bitmap(wuss, wuss_iconpool_lookup(wuss, "radoff"));
    if (bm == NULL) FAIL("no radoff");

    wuss_icon_spec_radio(&spec, (box_t) { 0, 0, 100, 30 }, "Pool", 0);
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    fg = wuss->palette[icon->spec.fg];

    /* the set's glyph is blitted at the left, the label follows it */
    {
      box_t glyph, label;

      glyph    = b;
      glyph.x1 = b.x0 + bm->size.w;
      label    = b;
      label.x0 = glyph.x1;
      if (count(&env, &glyph, WIPE) == (glyph.x1 - glyph.x0) * (b.y1 - b.y0) ||
          count(&env, &label, fg) == 0)
        FAIL("glyph from the set");
    }
    wuss_icon_delete(env.window, icon);
  }

  printf("test: menu entries: tick, arrow, swatches, hover, shortcuts\n");

  {
    int      fh, pad, h, cy, ax, ay;
    colour_t swatch;

    bmfont_get_info(font, NULL, &fh, NULL, NULL);
    pad = WUSS_MENU_ENTRY_PAD;
    h   = MAX(fh, 8);

    memset(&spec, 0, sizeof(spec));
    spec.type = wuss_ICON_TYPE_MENU_ENTRY;
    spec.bbox = (box_t) { 0, 0, 160, 20 };
    spec.text = "Entry";
    spec.fg   = wuss_COLOUR_BLACK;
    spec.bg   = wuss_COLOUR_WHITE;
    spec.flags = wuss_ICON_FLAGS_SUBMENU;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    fg = wuss->palette[icon->spec.fg];

    /* with no symbol font the tick and arrow fall back to vector strokes */
    cy = b.y0 + (b.y1 - b.y0 - h) / 2;
    ax = b.x1 - 1 - pad - ((fh >= 8) ? fh / 3 : 3);
    ay = b.y0 + (b.y1 - b.y0) / 2;
    if (px(&env, ax, ay) != pixel_of(&env, fg) ||
        px(&env, b.x0 + pad, cy + h / 2) != pixel_of(&env, white))
      FAIL("submenu arrow");

    wuss__icon_set_state(icon, wuss_ICON_STATE_SELECTED, 1);
    redraw(&env, icon);
    if (px(&env, b.x0 + pad, cy + h / 2) != pixel_of(&env, fg))
      FAIL("tick");

    /* hover swaps ink and ground over the text column only */
    wuss__icon_set_state(icon, wuss_ICON_STATE_HOVERED, 1);
    redraw(&env, icon);
    if (px(&env, b.x0 + wuss__menu_entry_gutter_left(fh), b.y0) !=
          pixel_of(&env, fg) ||
        px(&env, b.x0, b.y0) == pixel_of(&env, fg))
      FAIL("hover highlight");

    /* a disabled row ignores hover */
    icon->spec.flags |= wuss_ICON_FLAGS_DISABLED;
    redraw(&env, icon);
    if (px(&env, b.x0 + wuss__menu_entry_gutter_left(fh), b.y0) !=
          pixel_of(&env, white))
      FAIL("disabled row highlit");
    wuss_icon_delete(env.window, icon);

    /* a colour swatch chip, then the hatched "None" chip */
    spec.flags            = wuss_ICON_FLAGS_SWATCH;
    spec.u.menu_entry.swatch = wuss_COLOUR_RED;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    swatch = wuss->palette[icon->spec.u.menu_entry.swatch];
    if (count(&env, &b, swatch) == 0)
      FAIL("swatch chip");
    wuss_icon_delete(env.window, icon);

    spec.u.menu_entry.swatch = wuss_NO_BACKGROUND;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    if (count(&env, &b, colour_rgb(0, 0, 0)) == 0)
      FAIL("hatched none chip");
    wuss_icon_delete(env.window, icon);

    /* a shortcut keycap: filled when enabled, outlined when disabled */
    spec.flags                  = 0;
    spec.u.menu_entry.shortcut  = "^S";
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    {
      box_t right;
      int   filled;

      right    = b;
      right.x0 = b.x0 + (b.x1 - b.x0) / 2;
      filled   = count(&env, &right, dark);
      if (filled == 0)
        FAIL("shortcut keycap");

      icon->spec.flags |= wuss_ICON_FLAGS_DISABLED;
      redraw(&env, icon);
      if (count(&env, &right, dark) >= filled)
        FAIL("disabled keycap still filled");
    }
    wuss_icon_delete(env.window, icon);
  }

  printf("test: sliders: horizontal, reversed, vertical\n");

  {
    colour_t track, value;
    box_t    groove, inner;

    track = wuss->palette[wuss->slider.track];
    value = wuss->palette[wuss->slider.value];

    wuss_icon_spec_slider(&spec, (box_t) { 0, 0, 100, 20 },
                          wuss_SLIDER_HORIZONTAL, 0, 100, 50);
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    inner = box_grown(&b, -2);
    wuss__slider_groove_box(&inner, &groove);
    if (px(&env, groove.x0 + 1, groove.y0) != pixel_of(&env, value) ||
        px(&env, groove.x1 - 2, groove.y0) != pixel_of(&env, track))
      FAIL("horizontal slider");

    /* at its minimum nothing is filled */
    wuss_icon_set_value(env.window, icon, 0);
    redraw(&env, icon);
    if (count(&env, &groove, value) != 0)
      FAIL("empty slider has a fill");
    wuss_icon_delete(env.window, icon);

    /* min > max runs the fill the other way: 25 of 100..0 is three quarters */
    wuss_icon_spec_slider(&spec, (box_t) { 0, 0, 100, 20 },
                          wuss_SLIDER_HORIZONTAL, 100, 0, 25);
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    if (px(&env, groove.x0 + (groove.x1 - groove.x0) * 2 / 3, groove.y0) !=
          pixel_of(&env, value))
      FAIL("reversed slider");
    wuss_icon_delete(env.window, icon);

    wuss_icon_spec_slider(&spec, (box_t) { 0, 0, 20, 100 },
                          wuss_SLIDER_VERTICAL, 0, 100, 50);
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    inner = box_grown(&b, -2);
    wuss__slider_groove_box(&inner, &groove);
    if (px(&env, groove.x0, groove.y1 - 2) != pixel_of(&env, value) ||
        px(&env, groove.x0, groove.y0 + 1) != pixel_of(&env, track))
      FAIL("vertical slider");
    wuss_icon_delete(env.window, icon);
  }

  printf("test: writable: outline, selection, caret, disabled\n");

  {
    colour_t red;
    box_t    inner;

    red = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_RED)];

    wuss_icon_spec_writable(&spec, (box_t) { 0, 0, 100, 20 }, "abcdef", 16, 0);
    spec.bg = wuss_COLOUR_WHITE;
    rc = draw(&env, &spec, &icon, &b);
    if (rc != result_OK) goto Failure;
    fg    = wuss->palette[icon->spec.fg];
    inner = box_grown(&b, -1);
    if (px(&env, b.x0, b.y0) != pixel_of(&env, fg) ||
        count(&env, &inner, fg) == 0 || count(&env, &inner, red) != 0)
      FAIL("writable at rest");

    /* select "bcd" with the caret after it: a fg block plus a red caret */
    rc = wuss_set_focus(wuss, env.window);
    if (rc == result_OK)
      rc = wuss_icon_set_caret(env.window, icon, 4);
    if (rc != result_OK) goto Failure;
    wuss->caret.anchor = 1;
    {
      int before;

      before = count(&env, &inner, fg);
      redraw(&env, icon);
      if (count(&env, &inner, fg) <= before || count(&env, &inner, red) == 0)
        FAIL("writable selection and caret");
    }
    wuss->caret.anchor = wuss->caret.index;

    /* disabled: outline and text sink to bevel dark */
    wuss_icon_set_disabled(env.window, icon, 1);
    redraw(&env, icon);
    if (px(&env, b.x0, b.y0) != pixel_of(&env, dark) ||
        count(&env, &inner, fg) != 0)
      FAIL("disabled writable");
    wuss_icon_delete(env.window, icon);
  }

  printf("test: an icon scrolled wholly off its window draws nothing\n");

  wuss_icon_spec_label(&spec, (box_t) { 0, 0, 60, 16 }, "Hi", 0);
  spec.bg = wuss_COLOUR_WHITE;
  rc = draw(&env, &spec, &icon, &b);
  if (rc != result_OK) goto Failure;
  wipe(&env);
  wuss__icon_draw(wuss, env.window, icon, &env.content, POINT(1000, 1000));
  if (count(&env, &(box_t) { 0, 0, 240, 240 }, WIPE) != 240 * 240)
    FAIL("off-window icon touched the screen");
  wuss_icon_delete(env.window, icon);

  wuss_window_close(env.window);
  wuss_fixture_close(&fixture);
  bmfont_destroy(font);

  return result_TEST_PASSED;

Failure:
  printf("wuss_icon_draw_test: failed (rc=0x%X)\n", rc);
  wuss_fixture_close(&fixture);
  if (font != NULL)
    bmfont_destroy(font);

  return result_TEST_FAILED;
}

#else

result_t wuss_icon_draw_test(const char *resources)
{
  NOT_USED(resources);
  return result_TEST_PASSED;
}

#endif
