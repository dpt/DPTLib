/* wuss/test/tasks/icons.c -- work-area icons task */

#ifdef WUSS_APP
#include "framebuf/palettes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/bitmap.h"
#include "framebuf/palettes.h"
#include "framebuf/pattern.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "io/path.h"

#include "icons.h"

#define ICONS_DOC_W    260
#define ICONS_DOC_H    900 /* taller than the window, so scrolling is exercised */
#define ICONS_MARGIN   28  /* left edge of everything except frame captions */
#define ICONS_ROW      20  /* vertical pitch between stacked simple icons */

/* Maximum spec count across every group icons_create can lay out; the sprite
 * pair is the only optional part (skipped when the PNG fails to load). Kept
 * as one enum so a miscount between this and the groups below fails loudly
 * (an array bound, not a silent overrun) rather than corrupting the heap. */
enum
{
  ICONS_N_INTRO   = 4, /* heading, counter button, counter label, scrolled-away button */
  ICONS_N_BUTTONS = 4, /* frame + normal + default + disabled button */
  ICONS_N_RADIOS  = 8, /* frame + 3 radios + option + state label + 2 justified labels */
  ICONS_N_BITMAPS = 3, /* frame + decorative + interactive bitmap */
  ICONS_N_PATTERN = 2, /* frame + one PATTERN swatch */
  ICONS_N_MENU    = 7, /* plain, ticked, swatch, submenu, disabled, rule, separator entry */
  ICONS_NSPECS    = ICONS_N_INTRO + ICONS_N_BUTTONS + ICONS_N_RADIOS +
                    ICONS_N_BITMAPS + ICONS_N_PATTERN + ICONS_N_MENU
};

/* Running state threaded through the icons_add_* helpers: where to write the
 * next spec, and how far down the document the next group should start. */
typedef struct icons_layout
{
  wuss_icon_spec_t *specs;
  int               n;    /* specs[0..n) are filled in */
  int               y;    /* document y of the next group */
  wuss_colour_t     black;
  wuss_colour_t     grey5;
  wuss_colour_t     grey6;
  wuss_colour_t     red;  /* menu swatch demo colour */
}
icons_layout_t;

/* ----------------------------------------------------------------------- */

/* [0] a heading, [1] the button that bumps the counter (shown as a default
 * action button, so it carries the accent styling), [2] the counter label
 * beside it, [3] a button far down the document, to prove icons scroll and
 * stay clickable. Returns the indices of [1] and [2] via button/counter. */
static void icons_add_intro(icons_layout_t *lay, int *button, int *counter)
{
  wuss_icon_spec_t *s;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN, lay->y, 220, 14);
  s->type  = wuss_ICON_TYPE_LABEL;
  s->text  = "Work-area icons:";
  s->fg    = lay->black;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FONT(1);
  lay->n++;
  lay->y += 30;

  *button = lay->n;
  s       = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN, lay->y, 80, 22);
  s->type  = wuss_ICON_TYPE_BUTTON;
  s->text  = "Press me";
  s->fg    = lay->black;
  s->bg    = lay->grey6;
  s->flags = wuss_ICON_FLAGS_DEFAULT;
  lay->n++;

  *counter = lay->n;
  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 92, lay->y, 120, 22);
  s->type = wuss_ICON_TYPE_LABEL;
  s->text = "0";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;
  lay->y += 46;

  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, 780, 90, 52);
  s->type = wuss_ICON_TYPE_BUTTON;
  s->text = "Scrolled";
  s->fg   = lay->black;
  s->bg   = lay->grey6;
  lay->n++;
}

/* A grouping frame captioned "Buttons", with a normal, a default (accent) and
 * a disabled button side by side inside it. */
static void icons_add_buttons(icons_layout_t *lay)
{
  wuss_icon_spec_t *s;
  int               top;

  top     = lay->y;
  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top, 200, 56);
  s->type = wuss_ICON_TYPE_FRAME;
  s->text = "Buttons";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 22, 56, 22);
  s->type  = wuss_ICON_TYPE_BUTTON;
  s->text  = "Normal";
  s->fg    = lay->black;
  s->bg    = lay->grey6;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 74, top + 22, 56, 22);
  s->type  = wuss_ICON_TYPE_BUTTON;
  s->text  = "Default";
  s->fg    = lay->black;
  s->bg    = lay->grey6;
  s->flags = wuss_ICON_FLAGS_DEFAULT;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 138, top + 22, 56, 22);
  s->type  = wuss_ICON_TYPE_BUTTON;
  s->text  = "Disabled";
  s->fg    = lay->black;
  s->bg    = lay->grey6;
  s->flags = wuss_ICON_FLAGS_DISABLED;
  lay->n++;

  lay->y = top + 70;
}

/* A grouping frame captioned "Radios & options", with two justified labels,
 * three radios sharing group 1, a standalone option and a label echoing
 * whichever control last changed. Returns the indices of the option and the
 * echo label via opt/state. */
static void icons_add_radios(icons_layout_t *lay, int *opt, int *state)
{
  wuss_icon_spec_t *s;
  int               top;
  int               r;

  top     = lay->y;
  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top, 200, 170);
  s->type = wuss_ICON_TYPE_FRAME;
  s->text = "Radios & options";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 20, 180, 14);
  s->type  = wuss_ICON_TYPE_LABEL;
  s->text  = "right";
  s->fg    = lay->black;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 40, 180, 14);
  s->type  = wuss_ICON_TYPE_LABEL;
  s->text  = "centre";
  s->fg    = lay->black;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FLAGS_JUSTIFY_CENTRE;
  lay->n++;

  for (r = 0; r < 3; r++)
  {
    s        = &lay->specs[lay->n];
    s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 66 + r * 20, 180, 16);
    s->type  = wuss_ICON_TYPE_RADIO;
    s->text  = (r == 0) ? "Red" : (r == 1) ? "Green" : "Blue";
    s->fg    = lay->black;
    s->bg    = wuss_NO_BACKGROUND;
    s->group = 1;
    lay->n++;
  }

  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 130, 180, 16);
  s->type = wuss_ICON_TYPE_OPTION;
  s->text = "Wireframe";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  *opt    = lay->n;
  lay->n++;

  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 150, 180, 14);
  s->type = wuss_ICON_TYPE_LABEL;
  s->text = "(no selection)";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  *state  = lay->n;
  lay->n++;

  lay->y = top + 190;
}

/* A grouping frame captioned "Bitmaps", holding a decorative sprite and (to
 * its right) an interactive one that bumps the counter -- only laid out if
 * the sprite loaded. Returns the interactive icon's index via *hotspot, or
 * leaves it unset if sprite is NULL. */
static void icons_add_bitmaps(icons_layout_t *lay,
                              const bitmap_t *sprite,
                              int            *hotspot)
{
  wuss_icon_spec_t *s;
  int               top;

  if (sprite == NULL)
    return;

  top     = lay->y;
  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top, 200,
                                 sprite->size.h + 30);
  s->type = wuss_ICON_TYPE_FRAME;
  s->text = "Bitmaps";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;

  s         = &lay->specs[lay->n];
  s->bbox   = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 20,
                                   sprite->size.w, sprite->size.h);
  s->type   = wuss_ICON_TYPE_BITMAP;
  s->bitmap = sprite;
  lay->n++;

  s         = &lay->specs[lay->n];
  s->bbox   = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 20 + sprite->size.w, top + 20,
                                   sprite->size.w, sprite->size.h);
  s->type   = wuss_ICON_TYPE_BITMAP;
  s->bitmap = sprite;
  s->flags  = wuss_ICON_FLAGS_INTERACTIVE;
  *hotspot  = lay->n;
  lay->n++;

  lay->y = top + sprite->size.h + 50;
}

/* A grouping frame captioned "Pattern", holding one PATTERN-filled swatch. */
static void icons_add_pattern(icons_layout_t *lay)
{
  wuss_icon_spec_t *s;
  int               top;

  top     = lay->y;
  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top, 200, 60);
  s->type = wuss_ICON_TYPE_FRAME;
  s->text = "Pattern";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;

  s         = &lay->specs[lay->n];
  s->bbox   = (box_t) BOX_POS_SIZE(ICONS_MARGIN + 10, top + 20, 180, 26);
  s->type   = wuss_ICON_TYPE_PATTERN;
  s->fg     = lay->black;
  s->bg     = lay->grey6;
  s->pattern = screen_PATTERN_DIAGONAL;
  lay->n++;

  lay->y = top + 76;
}

/* A menu-entry strip: plain, ticked, a swatch entry, a submenu entry, a
 * disabled entry, then a dashed rule and a SEPARATOR-flagged entry below it.
 * Hover the pointer over any live entry to see the highlight track; the rule
 * stays inert. Returns the "Show grid" index (started ticked) via *ticked. */
static void icons_add_menu(icons_layout_t *lay, int *ticked)
{
  wuss_icon_spec_t *s;
  int               top;

  top     = lay->y;
  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top, 180, 16);
  s->type = wuss_ICON_TYPE_MENU_ENTRY;
  s->text = "Open";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;

  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top + ICONS_ROW, 180, 16);
  s->type = wuss_ICON_TYPE_MENU_ENTRY;
  s->text = "Show grid";
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  *ticked = lay->n;
  lay->n++;

  s         = &lay->specs[lay->n];
  s->bbox   = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top + ICONS_ROW * 2, 180, 16);
  s->type   = wuss_ICON_TYPE_MENU_ENTRY;
  s->text   = "Layer colour";
  s->fg     = lay->black;
  s->bg     = wuss_NO_BACKGROUND;
  s->swatch = lay->red;
  s->flags  = wuss_ICON_FLAGS_SWATCH;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top + ICONS_ROW * 3, 180, 16);
  s->type  = wuss_ICON_TYPE_MENU_ENTRY;
  s->text  = "Export";
  s->fg    = lay->black;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FLAGS_SUBMENU;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top + ICONS_ROW * 4, 180, 16);
  s->type  = wuss_ICON_TYPE_MENU_ENTRY;
  s->text  = "Disabled";
  s->fg    = lay->black;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FLAGS_DISABLED;
  lay->n++;

  s       = &lay->specs[lay->n];
  s->bbox = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top + ICONS_ROW * 5, 180, 10);
  s->type = wuss_ICON_TYPE_RULE;
  s->fg   = lay->black;
  s->bg   = wuss_NO_BACKGROUND;
  lay->n++;

  s        = &lay->specs[lay->n];
  s->bbox  = (box_t) BOX_POS_SIZE(ICONS_MARGIN, top + ICONS_ROW * 6, 180, 16);
  s->type  = wuss_ICON_TYPE_MENU_ENTRY;
  s->text  = "Quit";
  s->fg    = lay->black;
  s->bg    = wuss_NO_BACKGROUND;
  s->flags = wuss_ICON_FLAGS_SEPARATOR;
  lay->n++;

  lay->y = top + ICONS_ROW * 7 + 10;
}

/* ----------------------------------------------------------------------- */

result_t icons_create(wuss_t       *wuss,
                      bmfont_t     *font,
                      const char   *resources,
                      icons_task_t *task)
{
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  wuss_icon_spec_t specs[ICONS_NSPECS];
  wuss_icon_t     *made[ICONS_NSPECS];
  icons_layout_t   lay;
  const char      *sprite_path;
  int              i_button, i_counter, i_opt, i_state, i_hotspot, i_ticked;
  result_t         rc;

  task->font       = font;
  task->label      = colour_rgb(0x00, 0x00, 0x00);
  task->paper      = colour_rgb(0xDD, 0xDD, 0xDD); /* the window bg, below */
  task->window     = NULL;
  task->button     = NULL;
  task->counter    = NULL;
  task->count      = 0;
  task->opt        = NULL;
  task->state      = NULL;
  task->has_sprite = 0;
  task->hotspot    = NULL;

  sprite_path = path_join_filename(resources, 3, "resources", "wuss",
                                   path_join_leafname("ninepatch", "png"));
  if (bitmap_load_png(&task->sprite, sprite_path) == result_OK)
    task->has_sprite = 1;

  delegate_desc.handle    = icons_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "icons";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    if (task->has_sprite)
      free(task->sprite.base);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  memset(&lay, 0, sizeof(lay));
  lay.black = wuss_nearest_colour(wuss, 0x00, 0x00, 0x00);
  lay.grey5 = wuss_nearest_colour(wuss, 0xBB, 0xBB, 0xBB);
  lay.grey6 = wuss_nearest_colour(wuss, 0xDD, 0xDD, 0xDD);
  lay.red   = wuss_nearest_colour(wuss, 0xCC, 0x33, 0x33);

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(ICONS_DOC_W, 200),
                                 "Icons",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_PATTERN(lay.grey5,
                                                       screen_PATTERN_CROSSHATCH,
                                                       lay.grey6),
                                 SIZE2D(ICONS_DOC_W, ICONS_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the task block and its sprite */
    return rc;
  }

  memset(specs, 0, sizeof(specs));
  lay.specs = specs;
  lay.n     = 0;
  lay.y     = 28; /* icons sit past the ruler gutter -- see ICONS_GUTTER-ish
                    * axis labels drawn by icons_redraw, which keep the top/
                    * left strip to themselves */

  i_hotspot = -1;

  icons_add_intro(&lay, &i_button, &i_counter);
  icons_add_buttons(&lay);
  icons_add_radios(&lay, &i_opt, &i_state);
  icons_add_bitmaps(&lay, task->has_sprite ? &task->sprite : NULL, &i_hotspot);
  icons_add_pattern(&lay);
  icons_add_menu(&lay, &i_ticked);

  rc = wuss_icon_create_array(task->window, specs, lay.n, made);
  if (rc != result_OK)
    goto failure;

  task->button  = made[i_button];
  task->counter = made[i_counter];
  task->opt     = made[i_opt];
  task->state   = made[i_state];
  if (i_hotspot >= 0)
    task->hotspot = made[i_hotspot];
  wuss_icon_set_selected(made[i_ticked], 1); /* "Show grid" starts ticked */

  /* fully built: from here a last-window close reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(delegate, 1);

  return result_OK;

failure:
  wuss_task_destroy(delegate); /* closes the window; QUIT frees block + sprite */
  return rc;
}

#define ICONS_GRID       16 /* document-space pitch of the backdrop grid */
#define ICONS_AXIS_LABEL 64 /* label every Nth grid line along each axis */

/* Screen coordinate of the first grid line at or after lo. Grid lines sit at
 * document multiples of ICONS_GRID; the screen coordinate of document d is
 * origin - scroll + d, so lines fall on screen coordinates congruent to
 * (origin - scroll) modulo ICONS_GRID. */
static int icons_grid_first(int origin, int scroll, int lo)
{
  int phase;

  phase = (origin - scroll) % ICONS_GRID;
  if (phase < 0)
    phase += ICONS_GRID;

  return lo - ((lo - phase) % ICONS_GRID + ICONS_GRID) % ICONS_GRID;
}

static result_t icons_redraw(const wuss_event_t *event, void *task_data)
{
  icons_task_t *tcx;
  screen_t     *scr;
  const box_t  *content;
  const box_t  *bounds;
  point_t       scroll;
  point_t       pos;
  char          buf[16];
  int           ox;     /* screen x of document x=0 */
  int           oy;     /* screen y of document y=0 */
  int           doc;
  int           x;
  int           y;

  tcx = task_data;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;
  scroll  = event->data.redraw.scroll;

  ox = bounds->x0 - scroll.x;
  oy = bounds->y0 - scroll.y;

  /* Everything this task paints is anchored to the document, not the window,
   * so it scrolls rigidly with the content -- which is what Wuss's scroll
   * blit assumes. Nothing here is pinned to a window edge. Every draw is
   * clipped to the dirty rectangle (content) so partial redraws stay cheap. */

  /* the faint crosshatch backdrop is now the window's own bg (a
   * wuss_backdrop_t pattern fill), painted by Wuss before this event and
   * phase-locked to the scroll origin -- nothing to draw here */

  /* x-axis ruler: document x printed just below the y=0 line, at each
   * labelled grid column. Scrolls with the document like the grid. */
  for (x = icons_grid_first(bounds->x0, scroll.x, content->x0);
       x < content->x1;
       x += ICONS_GRID)
  {
    doc = x - ox;
    if (doc <= 0 || doc % ICONS_AXIS_LABEL != 0)
      continue;

    snprintf(buf, sizeof(buf), "%d", doc);
    pos = POINT(x + 2, oy + 2);
    bmfont_draw(tcx->font, scr, buf, (int) strlen(buf),
                tcx->label, tcx->paper, &pos, NULL);
  }

  /* y-axis ruler: document y printed just right of the x=0 line, at each
   * labelled grid row. */
  for (y = icons_grid_first(bounds->y0, scroll.y, content->y0);
       y < content->y1;
       y += ICONS_GRID)
  {
    doc = y - oy;
    if (doc <= 0 || doc % ICONS_AXIS_LABEL != 0)
      continue;

    snprintf(buf, sizeof(buf), "%d", doc);
    pos = POINT(ox + 2, y + 2);
    bmfont_draw(tcx->font, scr, buf, (int) strlen(buf),
                tcx->label, tcx->paper, &pos, NULL);
  }

  return result_OK;
}

static result_t icons_icon(const wuss_event_t *event, void *task_data)
{
  icons_task_t *tcx;
  wuss_icon_t  *icon;
  char          buf[48];

  tcx  = task_data;
  icon = event->data.icon.icon;

  /* a radio/option latches on MOUSE_UP -- report the state then */
  if (event->data.icon.action == wuss_MOUSE_UP &&
      (wuss_icon_get_type(icon) == wuss_ICON_TYPE_RADIO ||
       wuss_icon_get_type(icon) == wuss_ICON_TYPE_OPTION))
  {
    snprintf(buf, sizeof(buf), "%s: %s%s",
             wuss_icon_get_text(icon),
             wuss_icon_get_selected(icon) ? "on" : "off",
             (icon == tcx->opt) ? "" : " (radio)");
    return wuss_icon_set_text(tcx->state, buf);
  }

  if (event->data.icon.action != wuss_MOUSE_DOWN)
    return result_OK;
  if (icon != tcx->button && icon != tcx->hotspot)
    return result_OK;

  tcx->count++;
  snprintf(buf, sizeof(buf), "%d", tcx->count);

  return wuss_icon_set_text(tcx->counter, buf);
}

result_t icons_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  icons_task_t *tcx;

  tcx = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return icons_redraw(event, task_data);

  case wuss_EVENT_ICON:
    return icons_icon(event, task_data);

  case wuss_EVENT_QUIT:
    if (tcx->has_sprite)
      free(tcx->sprite.base);
    free(tcx); /* calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
