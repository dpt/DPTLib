/* wuss/test/tasks/palette.c -- desktop palette swatch grid task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"
#include "io/namelist.h"
#include "io/path.h"
#include "wuss/menu.h"

#include "palette.h"

#define PALETTE_HEX_EXT     ".hex"
#define PALETTE_NCOLOURS    16 /* one colour_t[] row per *.hex file; the
                                * system palette wuss_create was given is
                                * fixed at this length */

/* index of the "Invert" row in the picker menu, after one row per *.hex
 * file and the dashed rule above it */
#define PALETTE_MENU_INVERT_INDEX(pc) ((pc)->nnames)

/* ----------------------------------------------------------------------- */
/* Parse a *.hex file: one "rrggbb" line per colour, no leading '#'. Fails
 * (leaving *out untouched) unless exactly PALETTE_NCOLOURS well-formed lines
 * are read, so a malformed or wrongly-sized file can never desync from the
 * fixed-length system palette wuss was created with. */

result_t palette_load_hex(const char *resources,
                          const char *name,
                          colour_t   *out)
{
  const char *leaf;
  const char *path;
  char        pathbuf[DPTLIB_MAXPATH];
  FILE       *fp;
  char        line[16];
  int         i;
  unsigned    r, g, b;

  leaf = path_join_leafname(name, "hex");
  path = path_join_filename(resources, 3, "resources", "palettes", leaf);
  strcpy(pathbuf, path); /* path_join_filename's buffer is reused by fopen */

  fp = fopen(pathbuf, "r");
  if (fp == NULL)
    return result_FILE_NOT_FOUND;

  for (i = 0; i < PALETTE_NCOLOURS; i++)
  {
    if (fgets(line, sizeof(line), fp) == NULL ||
        sscanf(line, "%2x%2x%2x", &r, &g, &b) != 3)
    {
      fclose(fp);
      return result_WUSS_BAD_COLOUR;
    }
    out[i] = colour_rgb((int) r, (int) g, (int) b);
  }

  fclose(fp);
  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t palette_create(wuss_t          *wuss,
                        const char      *resources,
                        const char      *startup_name,
                        palette_task_t **out)
{
  result_t         rc;
  palette_task_t  *task;
  wuss_task_desc_t delegate_desc;
  const char      *dir;
  char             dirbuf[DPTLIB_MAXPATH];
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss      = wuss;
  task->resources = resources;
  task->invert    = false;
  task->nnames    = 0;
  task->selected  = 0;

  dir = path_join_filename(resources, 2, "resources", "palettes");
  strcpy(dirbuf, dir); /* path_join_filename's buffer is reused by the scan */
  namelist_scan(dirbuf, PALETTE_HEX_EXT, task->names[0],
                sizeof(task->names[0]), PALETTE_MAX_FILES, 1 /* sorted */,
                &task->nnames);

  if (startup_name != NULL)
    for (i = 0; i < task->nnames; i++)
      if (strcmp(task->names[i], startup_name) == 0)
      {
        task->selected = i;
        break;
      }

  /* built once; ticks are refreshed from task->selected/invert on each open */
  for (i = 0; i < task->nnames; i++)
  {
    task->menu_items[i].text    = task->names[i];
    task->menu_items[i].submenu = NULL;
    task->menu_items[i].window  = NULL;
  }
  task->menu_items[PALETTE_MENU_INVERT_INDEX(task)].text    = "Invert";
  task->menu_items[PALETTE_MENU_INVERT_INDEX(task)].submenu = NULL;
  task->menu_items[PALETTE_MENU_INVERT_INDEX(task)].window  = NULL;
  task->menu_items[PALETTE_MENU_INVERT_INDEX(task)].flags   = wuss_MENU_ITEM_DASHED;
  task->menu.title  = "Palette";
  task->menu.items  = task->menu_items;
  task->menu.nitems = task->nnames + 1;

  /* backdrop for any rounding gap around the grid */
  delegate_desc.handle    = palette_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "palette";
  rc = wuss_task_create(wuss, &delegate_desc, &task->delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = wuss_window_create_placed(task->delegate,
                                 SIZE2D(100, 100),
                                 "Palette",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT, /* swatch grid is laid out across the whole window, so a resize must redraw all of it, not just the newly (un)covered edge */
                                 wuss_BACKDROP_COLOUR(palette_PICO8_BLACK),
                                 SIZE2D(100, 100),
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = wuss_window_create_placed(task->delegate,
                                 SIZE2D(100, 100),
                                 "Screen",
                                 wuss_WINDOW_DEFAULT | wuss_WINDOW_NO_RESIZE_BLIT,
                                 wuss_BACKDROP_COLOUR(palette_PICO8_BLACK),
                                 SIZE2D(100, 100),
                                 SIZE2D(0, 0),
                                 &task->window2);
  if (rc != result_OK)
  {
    wuss_task_destroy(task->delegate); /* closes "Palette", QUIT frees the block */
    return rc;
  }

  /* both windows up: from here, closing the last one reaps the task and its
   * wuss_EVENT_QUIT frees task_data */
  wuss_task_set_autoclose(task->delegate, 1);

  if (out)
    *out = task;

  return result_OK;
}

void palette_destroy(palette_task_t *task)
{
  free(task);
}

/* shared by both windows: paint a roughly-square grid of npalette swatches
 * across bounds */
static void palette_draw_grid(screen_t       *scr,
                              const box_t    *bounds,
                              int             sx,
                              int             sy,
                              const colour_t *palette,
                              int             npalette)
{
  int cols, rows;
  int cell_w, cell_h;
  int i;

  cols = 1;
  while (cols * cols < npalette)
    cols++;
  rows = (npalette + cols - 1) / cols;

  cell_w = (bounds->x1 - bounds->x0) / cols;
  cell_h = (bounds->y1 - bounds->y0) / rows;

  for (i = 0; i < npalette; i++)
  {
    int col, row, x, y;

    col = i % cols;
    row = i / cols;
    x   = bounds->x0 - sx + col * cell_w;
    y   = bounds->y0 - sy + row * cell_h;

    screen_fill_rect(scr, x, y, SIZE2D(cell_w, cell_h), palette[i]);
  }
}

static result_t palette_redraw(const wuss_event_t *event, void *task_data)
{
  palette_task_t *pc;
  const colour_t *palette;
  int             npalette;
  screen_t       *scr;
  const box_t    *bounds;
  int             sx, sy;

  pc      = task_data;
  palette = wuss_get_palette(pc->wuss, &npalette);

  if (npalette <= 0)
    return result_OK;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  palette_draw_grid(scr, bounds, sx, sy, palette, npalette);

  return result_OK;
}

/* "Screen" window: the physical screen bitmap's own palette, whatever size
 * its pixel format needs (2/4/16/256 for 1/2/4/8bpp); 32bpp has none, so
 * just label it */
static result_t palette_redraw_screen(palette_task_t     *pc,
                                      const wuss_event_t *event)
{
  screen_t    *scr;
  const box_t *bounds;
  int          sx, sy, npalette;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  if (scr->palette == NULL)
  {
    static const char label[] = "32bpp (none)";

    bmfont_t          *font   = wuss_get_font(pc->wuss);
    colour_t           ink    = colour_rgb(0xFF, 0xFF, 0xFF);
    colour_t           bg     = colour_rgba(0, 0, 0, 0); /* transparent */

    screen_fill_rect(scr, bounds->x0 - sx, bounds->y0 - sy,
                     SIZE2D(bounds->x1 - bounds->x0, bounds->y1 - bounds->y0),
                     colour_rgb(0x00, 0x00, 0x00));
    if (font != NULL)
    {
      int     ascent;
      point_t pos;

      bmfont_get_info(font, NULL, NULL, &ascent, NULL);
      pos = POINT(bounds->x0 - sx + 2, bounds->y0 - sy + 2 + ascent);
      wuss_text_draw(pc->wuss, 0, scr, label, (int) strlen(label), ink, bg,
                     &pos, NULL);
    }
    return result_OK;
  }

  npalette = 1 << (1 << pixelfmt_log2bpp(scr->format));

  palette_draw_grid(scr, bounds, sx, sy, scr->palette, npalette);

  return result_OK;
}

/* Set the picker menu's ticks from task->selected/invert, then open it. The
 * menu itself is built once by palette_create and lives in *pc, so it
 * outlives the open chain as wuss_menu_open requires. The invert row's
 * permanent DASHED flag is set at create time; wuss_menu_open_ticked only
 * touches the TICKED bit. */
static result_t palette_menu_open(palette_task_t *pc)
{
  unsigned int ticks;

  ticks = 1u << pc->selected;
  if (pc->invert)
    ticks |= 1u << PALETTE_MENU_INVERT_INDEX(pc);

  return wuss_menu_open_ticked(pc->delegate, &pc->menu, ticks,
                               wuss_get_pointer(pc->wuss), &pc->menu_handle);
}

static result_t palette_click(palette_task_t *pc, const wuss_event_t *event)
{
  if (event->data.mouse.action != wuss_MOUSE_DOWN)
    return result_OK;

  if (!(event->data.mouse.button & wuss_BUTTON_MENU))
    return result_OK;

  return palette_menu_open(pc);
}

/* A pick loads that *.hex file (or re-applies the current one, for a bare
 * Invert toggle) and, on success, installs it as the live system palette via
 * wuss_set_palette -- every task, including whichever one owns the
 * framebuffer bitmap and any physical palette, sees the change via the
 * resulting wuss_EVENT_PALETTE and reads the new array back with
 * wuss_get_palette. A load failure is silently ignored: the picker just
 * stays on the previous selection. Ticks are also updated in place via
 * wuss_menu_tick_item_live, so an ADJUST pick (which keeps the chain open)
 * shows the new tick without the menu being rebuilt or moved. */
static result_t palette_menu_select(palette_task_t     *pc,
                                    const wuss_event_t *event)
{
  result_t rc;
  int      index;
  int      old;
  colour_t loaded[PALETTE_NCOLOURS];
  int      i;

  if (event->data.menu_select.menu != &pc->menu)
    return result_OK;

  index = event->data.menu_select.index;

  if (index == PALETTE_MENU_INVERT_INDEX(pc))
  {
    pc->invert = !pc->invert;
    if (event->data.menu_select.button & wuss_BUTTON_ADJUST)
      wuss_menu_tick_item_live(pc->menu_handle, &pc->menu, index, pc->invert);
  }
  else if (index >= 0 && index < pc->nnames)
  {
    old          = pc->selected;
    pc->selected = index;
    pc->invert   = false;
    if (event->data.menu_select.button & wuss_BUTTON_ADJUST)
    {
      wuss_menu_tick_item_live(pc->menu_handle, &pc->menu, old, 0);
      wuss_menu_tick_item_live(pc->menu_handle, &pc->menu, index, 1);
      wuss_menu_tick_item_live(pc->menu_handle, &pc->menu,
                               PALETTE_MENU_INVERT_INDEX(pc), 0);
    }
  }
  else
    return result_OK;

  /* SELECT (as opposed to ADJUST) has already closed and freed the chain by
   * the time this event arrives; the handle is stale, don't touch it */
  if (!wuss_menu_should_keep_open(event))
    pc->menu_handle = NULL;

  if (pc->nnames == 0)
    return result_OK;

  rc = palette_load_hex(pc->resources, pc->names[pc->selected], loaded);
  if (rc != result_OK)
    return result_OK;

  if (pc->invert)
    for (i = 0; i < PALETTE_NCOLOURS; i++)
      loaded[i].primary ^= 0x00FFFFFFu;

  wuss_set_palette(pc->wuss, loaded, PALETTE_NCOLOURS);

  return result_OK;
}

result_t palette_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  palette_task_t *pc;

  pc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    if (window == pc->window2)
      return palette_redraw_screen(pc, event);
    return palette_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return palette_click(task_data, event);

  case wuss_EVENT_MENU_SELECT:
    return palette_menu_select(task_data, event);

  case wuss_EVENT_MENU_CLOSED:
    pc->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == pc->window2)
      pc->window2 = NULL;
    else
      pc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    /* one calloc'd block backs both windows; the task autocloses once the
     * second one goes, so free it here */
    palette_destroy(pc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
