/* wuss/test/tasks/icons.c -- work-area icons task */

#ifdef USE_SDL
#include "framebuf/palettes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "framebuf/bitmap.h"
#include "framebuf/palettes.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "io/path.h"

#include "icons.h"

#define ICONS_DOC_W 220
#define ICONS_DOC_H 640 /* taller than the window, so scrolling is exercised */

result_t icons_create(wuss_t       *wuss,
                      bmfont_t     *font,
                      const char   *resources,
                      icons_task_t *task)
{
  /* [0..3] heading, counter button, counter label and a scrolled-away button,
   * then [.. +3] a grouping frame with two differently-justified labels inside
   * it, then [.. +5] three grouped radios, a standalone option and a label
   * echoing the selection, then [.. +2] a decorative bitmap icon and an
   * interactive one that bumps the counter, then [.. +4] a strip of menu-entry
   * icons that hover-highlight */
  enum { ICONS_NSPECS = 4 + 3 + 5 + 2 + 5 };
  wuss_task_t      delegate;
  wuss_colour_t    black, grey5, grey6;
  wuss_icon_spec_t specs[ICONS_NSPECS];
  wuss_icon_t     *made[ICONS_NSPECS];
  const char      *sprite_path;
  int              g;      /* index of the first frame spec */
  int              r;      /* radio index */
  int              m;      /* index of the first menu-entry spec */
  int              nspecs; /* live spec count (sprite icons are optional) */
  result_t         rc;

  black = wuss_nearest_colour(wuss, 0x00, 0x00, 0x00);
  grey5 = wuss_nearest_colour(wuss, 0xBB, 0xBB, 0xBB);
  grey6 = wuss_nearest_colour(wuss, 0xDD, 0xDD, 0xDD);

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
                                   path_join_leafname("9tile", "png"));
  if (bitmap_load_png(&task->sprite, sprite_path) == result_OK)
    task->has_sprite = 1;

  delegate = wuss_task_start(icons_handle, task);

  rc = wuss_window_create_placed(wuss,
                                 SIZE2D(ICONS_DOC_W, 160),
                                 "Icons",
                                 wuss_WINDOW_NONE,
                                 wuss_BACKDROP_PATTERN(grey5,
                                                       screen_PATTERN_CROSSHATCH,
                                                       grey6),
                                 &delegate,
                                 SIZE2D(ICONS_DOC_W, ICONS_DOC_H),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    if (task->has_sprite)
      free(task->sprite.base);
    return rc;
  }

  memset(specs, 0, sizeof(specs));

  /* icons sit past the ruler gutter (see ICONS_GUTTER in icons_redraw) so the
   * axis labels have the top/left strip to themselves */

  /* [0] a heading label */
  specs[0].bbox = (box_t) BOX_POS_SIZE(28, 28, 180, 14);
  specs[0].type = wuss_ICON_TYPE_LABEL;
  specs[0].text = "Work-area icons:";
  specs[0].fg   = black;
  specs[0].bg   = wuss_NO_BACKGROUND;

  /* [1] the button that bumps the counter -- a default action button, so it
   * shows the accent styling */
  specs[1].bbox  = (box_t) BOX_POS_SIZE(28, 50, 80, 22);
  specs[1].type  = wuss_ICON_TYPE_BUTTON;
  specs[1].text  = "Press me";
  specs[1].fg    = black;
  specs[1].bg    = grey6;
  specs[1].flags = wuss_ICON_FLAGS_DEFAULT;

  /* [2] the counter label beside it */
  specs[2].bbox = (box_t) BOX_POS_SIZE(120, 50, 120, 22);
  specs[2].type = wuss_ICON_TYPE_LABEL;
  specs[2].text = "0";
  specs[2].fg   = black;
  specs[2].bg   = wuss_NO_BACKGROUND;

  /* [3] a button far down the document, to prove icons scroll and stay clickable */
  specs[3].bbox = (box_t) BOX_POS_SIZE(28, 460, 90, 52);
  specs[3].type = wuss_ICON_TYPE_BUTTON;
  specs[3].text = "Scrolled";
  specs[3].fg   = black;
  specs[3].bg   = grey6;

  /* [g] a grouping frame down the document, with [g+1] a right-justified and
   * [g+2] a centred label sat inside it */
  g = 4;

  specs[g].bbox = (box_t) BOX_POS_SIZE(28, 300, 170, 70);
  specs[g].type = wuss_ICON_TYPE_FRAME;
  specs[g].text = "Grouping frame";
  specs[g].fg   = black;
  specs[g].bg   = wuss_NO_BACKGROUND;

  specs[g + 1].bbox  = (box_t) BOX_POS_SIZE(38, 320, 150, 14);
  specs[g + 1].type  = wuss_ICON_TYPE_LABEL;
  specs[g + 1].text  = "right";
  specs[g + 1].fg    = black;
  specs[g + 1].bg    = wuss_NO_BACKGROUND;
  specs[g + 1].flags = wuss_ICON_FLAGS_JUSTIFY_RIGHT;

  specs[g + 2].bbox  = (box_t) BOX_POS_SIZE(38, 342, 150, 14);
  specs[g + 2].type  = wuss_ICON_TYPE_LABEL;
  specs[g + 2].text  = "centre";
  specs[g + 2].fg    = black;
  specs[g + 2].bg    = wuss_NO_BACKGROUND;
  specs[g + 2].flags = wuss_ICON_FLAGS_JUSTIFY_CENTRE;

  /* [g+3..g+5] three radios sharing group 1, [g+6] a standalone option,
   * [g+7] a label echoing whichever control last changed */
  for (r = 0; r < 3; r++)
  {
    specs[g + 3 + r].bbox  = (box_t) BOX_POS_SIZE(28, 380 + r * 20, 150, 16);
    specs[g + 3 + r].type  = wuss_ICON_TYPE_RADIO;
    specs[g + 3 + r].text  = (r == 0) ? "Red" : (r == 1) ? "Green" : "Blue";
    specs[g + 3 + r].fg    = black;
    specs[g + 3 + r].bg    = wuss_NO_BACKGROUND;
    specs[g + 3 + r].group = 1;
  }

  specs[g + 6].bbox = (box_t) BOX_POS_SIZE(28, 444, 150, 16);
  specs[g + 6].type = wuss_ICON_TYPE_OPTION;
  specs[g + 6].text = "Wireframe";
  specs[g + 6].fg   = black;
  specs[g + 6].bg   = wuss_NO_BACKGROUND;

  specs[g + 7].bbox = (box_t) BOX_POS_SIZE(28, 464, 180, 14);
  specs[g + 7].type = wuss_ICON_TYPE_LABEL;
  specs[g + 7].text = "(no selection)";
  specs[g + 7].fg   = black;
  specs[g + 7].bg   = wuss_NO_BACKGROUND;

  nspecs = g + 8;

  /* [g+8] a decorative bitmap, [g+9] an interactive one that bumps the
   * counter -- only if the sprite loaded */
  if (task->has_sprite)
  {
    specs[g + 8].bbox   = (box_t) BOX_POS_SIZE(28, 484, task->sprite.size.w,
                                               task->sprite.size.h);
    specs[g + 8].type   = wuss_ICON_TYPE_BITMAP;
    specs[g + 8].bitmap = &task->sprite;

    specs[g + 9].bbox   = (box_t) BOX_POS_SIZE(120, 484, task->sprite.size.w,
                                               task->sprite.size.h);
    specs[g + 9].type   = wuss_ICON_TYPE_BITMAP;
    specs[g + 9].bitmap = &task->sprite;
    specs[g + 9].flags  = wuss_ICON_FLAGS_INTERACTIVE;

    nspecs = g + 10;
  }

  /* [nspecs..nspecs+4] a menu-entry strip: plain, ticked, submenu, then a
   * standalone dashed rule and a SEPARATOR-flagged entry below it. Hover the
   * pointer over them to see the highlight track; the rule stays inert. */
  m = nspecs;

  specs[m].bbox     = (box_t) BOX_POS_SIZE(28, 524, 160, 16);
  specs[m].type     = wuss_ICON_TYPE_MENU_ENTRY;
  specs[m].text     = "Open";
  specs[m].fg       = black;
  specs[m].bg       = wuss_NO_BACKGROUND;

  specs[m + 1].bbox = (box_t) BOX_POS_SIZE(28, 542, 160, 16);
  specs[m + 1].type = wuss_ICON_TYPE_MENU_ENTRY;
  specs[m + 1].text = "Show grid";
  specs[m + 1].fg   = black;
  specs[m + 1].bg   = wuss_NO_BACKGROUND;

  specs[m + 2].bbox  = (box_t) BOX_POS_SIZE(28, 560, 160, 16);
  specs[m + 2].type  = wuss_ICON_TYPE_MENU_ENTRY;
  specs[m + 2].text  = "Export";
  specs[m + 2].fg    = black;
  specs[m + 2].bg    = wuss_NO_BACKGROUND;
  specs[m + 2].flags = wuss_ICON_FLAGS_SUBMENU;

  specs[m + 3].bbox  = (box_t) BOX_POS_SIZE(28, 578, 160, 10);
  specs[m + 3].type  = wuss_ICON_TYPE_RULE;
  specs[m + 3].fg    = black;
  specs[m + 3].bg    = wuss_NO_BACKGROUND;

  specs[m + 4].bbox  = (box_t) BOX_POS_SIZE(28, 588, 160, 16);
  specs[m + 4].type  = wuss_ICON_TYPE_MENU_ENTRY;
  specs[m + 4].text  = "Quit";
  specs[m + 4].fg    = black;
  specs[m + 4].bg    = wuss_NO_BACKGROUND;
  specs[m + 4].flags = wuss_ICON_FLAGS_SEPARATOR;

  nspecs = m + 5;

  rc = wuss_icon_create_array(task->window, specs, nspecs, made);
  if (rc != result_OK)
    goto failure;

  task->button  = made[1];
  task->counter = made[2];
  task->opt     = made[g + 6];
  task->state   = made[g + 7];
  if (task->has_sprite)
    task->hotspot = made[g + 9];
  wuss_icon_set_selected(made[m + 1], 1); /* "Show grid" starts ticked */

  return result_OK;

failure:
  wuss_window_close(task->window);
  task->window = NULL;
  if (task->has_sprite)
    free(task->sprite.base);
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

  case wuss_EVENT_CLOSE:
    wuss_window_close(window); /* frees the icons, which only borrowed sprite */
    if (tcx->has_sprite)
      free(tcx->sprite.base);
    free(tcx); /* calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* USE_SDL */
