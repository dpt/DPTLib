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
#define PALETTE_NCOLOURS    wuss_SYSTEM_PALETTE_LENGTH /* one colour_t[] row
                                * per *.hex file; the system palette
                                * wuss_create was given is fixed at this
                                * length */

/* indices of the top-level menu's fixed rows */
#define PALETTE_MENU_INFO_INDEX    0
#define PALETTE_MENU_LOAD_INDEX    1
#define PALETTE_MENU_INVERT_INDEX  2

/* ----------------------------------------------------------------------- */
/* Parse a *.hex file: one "rrggbb" line per colour, no leading '#'. Fails
 * (leaving *out untouched) unless exactly PALETTE_NCOLOURS well-formed lines
 * are read, so a malformed or wrongly-sized file can never desync from the
 * fixed-length system palette wuss was created with. */

result_t palette_load_hex(const char *resources,
                          const char *name,
                          colour_t   *out)
{
  const char *path;
  FILE       *fp;
  char        line[16];
  int         i;
  unsigned    r, g, b;

  path = pathf("%s/resources/palettes/%s.hex", resources, name);

  fp = fopen(path, "r");
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

result_t palette_create(wuss_t *wuss, palette_task_t **out)
{
  result_t         rc;
  palette_task_t  *task;
  wuss_task_desc_t delegate_desc;
  const char      *resources;
  const char      *dir;
  const colour_t  *current;
  int              ncurrent;
  int              i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss      = wuss;
  task->invert    = false;
  task->nnames    = 0;
  task->selected  = 0;

  resources = wuss_get_resources(wuss);
  dir       = pathf("%s/resources/palettes", resources);
  namelist_scan(dir, PALETTE_HEX_EXT, task->names[0],
                sizeof(task->names[0]), PALETTE_MAX_FILES, 1 /* sorted */,
                &task->nnames);

  /* tick whichever *.hex file matches wuss's current system palette, so the
   * picker starts in sync with what wuss_create actually loaded, without the
   * caller having to tell us its leafname separately */
  current = wuss_get_palette(wuss, &ncurrent);
  if (ncurrent == PALETTE_NCOLOURS)
    for (i = 0; i < task->nnames; i++)
    {
      colour_t candidate[PALETTE_NCOLOURS];

      if (palette_load_hex(resources, task->names[i], candidate) == result_OK &&
          memcmp(candidate, current, sizeof(candidate)) == 0)
      {
        task->selected = i;
        break;
      }
    }

  /* built once; ticks are refreshed from task->selected/invert on each open */
  for (i = 0; i < task->nnames; i++)
  {
    WUSS_MENU_ITEM(task->load_items, i, task->names[i],
                  wuss_MENU_ITEM_NONE);
  }
  WUSS_MENU_TITLE(task->load_menu, "Load", task->load_items, task->nnames);

  WUSS_MENU_ITEM_WINDOW(task->menu_items, PALETTE_MENU_INFO_INDEX, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * palette_menu_open */
  WUSS_MENU_ITEM_MENU(task->menu_items, PALETTE_MENU_LOAD_INDEX, "Load",
                      wuss_MENU_ITEM_NONE, &task->load_menu);
  WUSS_MENU_ITEM(task->menu_items, PALETTE_MENU_INVERT_INDEX,
                "Invert", wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->menu, "Palette", task->menu_items, 3);

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
  if (task->menu_handle != NULL)
    wuss_menu_close(task->menu_handle);
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
  static const wuss_proginfo_desc_t desc =
  {
    "Palette",
    "Desktop and screen palette grid",
    "(c) DPTLib contributors",
    "1.0 (" __DATE__ ")"
  };

  unsigned int ticks;

  wuss_proginfo_set_desc(&desc);
  pc->menu_items[PALETTE_MENU_INFO_INDEX].window =
    wuss_proginfo_window(pc->delegate);

  wuss_menu_tick_exclusive(&pc->load_menu, pc->selected);

  ticks = pc->invert ? 1u << PALETTE_MENU_INVERT_INDEX : 0;

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

  index = event->data.menu_select.index;

  if (event->data.menu_select.menu == &pc->menu &&
      index == PALETTE_MENU_INVERT_INDEX)
  {
    pc->invert = !pc->invert;
    if (event->data.menu_select.button & wuss_BUTTON_ADJUST)
      wuss_menu_tick_item_live(pc->menu_handle, &pc->menu, index, pc->invert);
  }
  else if (event->data.menu_select.menu == &pc->load_menu &&
          index >= 0 && index < pc->nnames)
  {
    old          = pc->selected;
    pc->selected = index;
    pc->invert   = false;
    if (event->data.menu_select.button & wuss_BUTTON_ADJUST)
    {
      wuss_menu_tick_item_live(pc->menu_handle, &pc->load_menu, old, 0);
      wuss_menu_tick_item_live(pc->menu_handle, &pc->load_menu, index, 1);
      wuss_menu_tick_item_live(pc->menu_handle, &pc->menu,
                               PALETTE_MENU_INVERT_INDEX, 0);
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

  rc = palette_load_hex(wuss_get_resources(pc->wuss), pc->names[pc->selected],
                        loaded);
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
    if (window != pc->window && window != pc->window2)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    return palette_click(task_data, event);

  case wuss_EVENT_MENU_SELECT:
    return palette_menu_select(task_data, event);

  case wuss_EVENT_MENU_CLOSED:
    pc->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == pc->menu_items[PALETTE_MENU_INFO_INDEX].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

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
