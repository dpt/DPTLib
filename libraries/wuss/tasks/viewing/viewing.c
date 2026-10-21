/* wuss/tasks/viewing/viewing.c -- static bitmap viewing task */

#ifdef WUSS_APP

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "wuss/menu.h"
#include "wuss/menu-desc.h"
#include "wuss/message.h"

#include "viewing.h"
#include "../taskapp.h"
#include "../common.h"

#define VIEWING_BORDERSZ 8 /* solid band drawn around the image */
#define VIEWING_MARGINSZ VIEWING_BORDERSZ
#define VIEWING_EMPTYSZ  64 /* image area before the first DataLoad */

/* rows of the main menu (its caption is not a row) */
enum { VIEWING_MENU_DITHERING, VIEWING_MENU_COLOURS, VIEWING_MENU_STAGE };

/* The window size that shows the image: with the stage, the smallest one
 * that fits it; without, the image in its band. */
static size2d_t viewing_size(const viewing_task_t *ic)
{
  stagebox_t boxes[stage_MAX_BOXES];
  size_t     nboxes;
  size2d_t   sz;

  if (ic->stage)
  {
    stage_generate(&ic->stagecfg, 0, 0,
                   ic->bitmap.size.w, ic->bitmap.size.h,
                   &sz.w, &sz.h, boxes, &nboxes);
    return sz;
  }

  sz.w = ic->bitmap.size.w + VIEWING_MARGINSZ * 2;
  sz.h = ic->bitmap.size.h + VIEWING_MARGINSZ * 2;

  return sz;
}

/* Resize the window and its document to fit the image */
static result_t viewing_fit(viewing_task_t *ic)
{
  result_t rc;
  size2d_t sz;

  sz = viewing_size(ic);

  rc = wuss_window_resize(ic->window, sz);
  if (rc != result_OK)
    return rc;

  return wuss_window_set_doc(ic->window, sz);
}

result_t viewing_create(wuss_t *wuss, viewing_task_t **out)
{
  /* centred, clear of the icon bar, rather than in the first free region */
  static const wuss_place_t place =
  {
    wuss_PLACE_AVOID_ICONBAR, wuss_ALIGN_CENTRE, NULL, { 0, 0 }
  };
  static const char *const colour_names[4] =
  {
    "Pasteboard", "Paper", "Shadow", "Border"
  };

  result_t         rc;
  viewing_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  size2d_t         sz;
  colour_t        *colour_fields[4];

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->bitmap.size = SIZE2D(VIEWING_EMPTYSZ, VIEWING_EMPTYSZ); /* no pixels */
  task->delegate    = NULL;
  task->menu        = NULL;
  task->menu_handle = NULL;
  task->dithering   = screen_DITHER_BLUE_NOISE;
  task->stage       = 1;
  stageconfig_init(&task->stagecfg);

  task->colour_pasteboard = colour_rgb(192, 192, 192); /* light grey */
  task->colour_paper      = colour_rgb(255, 255, 255);
  task->colour_shadow     = colour_rgb(96, 96, 96); /* dark grey */
  task->colour_border     = colour_rgb(0, 0, 0);
  colour_fields[0] = &task->colour_pasteboard;
  colour_fields[1] = &task->colour_paper;
  colour_fields[2] = &task->colour_shadow;
  colour_fields[3] = &task->colour_border;
  wuss_colourlist_init(&task->colours, wuss, 4, colour_fields, colour_names);

  /* indexed by screen_dither_t */
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_NONE, "None",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_BAYER, "Bayer",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_BLUE_NOISE, "Blue noise",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->dither_menu, "Dithering", task->dither_items,
                  NELEMS(task->dither_items));

  /* shows through the image's transparent pixels */
  delegate_desc.handle    = viewing_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "viewing";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: QUIT is delivered at wuss_destroy instead. Closing the
   * main window early leaks this block until then -- fine for a demo. */

  sz = viewing_size(task);

  rc = wuss_window_create_at(delegate,
                             &place,
                             SIZE2D(sz.w, sz.h),
                             "Viewing",
                             wuss_WINDOW_DEFAULT,
                             wuss_NO_BACKDROP,
                             sz,
                             SIZE2D(32, 32),
                             &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the bitmap and the block */
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

static result_t viewing_show(viewing_task_t *ic, const char *path);

result_t viewing_app_create(wuss_t *wuss)
{
  static const unsigned short opens[] = { filetype_RISCOS_PNG, 0 };
  static const taskapp_desc_t desc =
  {
    .name      = "Viewing",
    .purpose   = "View PNG images",
    .create    = (taskapp_create_fn_t) viewing_create,
    .data_load = (taskapp_data_load_fn_t) viewing_show,
    .drop_only = true,
    .opens     = opens
  };

  return taskapp_install(wuss, &desc);
}

void viewing_destroy(viewing_task_t *task)
{
  wuss_menu_destroy(task->menu);
  free(task->bitmap.base);
  free(task); /* task_data was calloc'd per instance by the spawner */
}

/* Draw the stage behind the image, centred in the window. The stage lays out
 * with y up, so flip it. */
static void viewing_redraw_stage(viewing_task_t *ic,
                                 screen_t       *scr,
                                 const box_t    *bounds,
                                 int             ox,
                                 int             oy)
{
  stagebox_t boxes[stage_MAX_BOXES];
  size_t     nboxes;
  size_t     i;
  int        w, h;
  box_t      b;
  colour_t   colour;

  stage_generate(&ic->stagecfg,
                 bounds->x1 - bounds->x0, bounds->y1 - bounds->y0,
                 ic->bitmap.size.w, ic->bitmap.size.h,
                 &w, &h, boxes, &nboxes);

  for (i = 0; i < nboxes; i++)
  {
    b = (box_t) { ox + boxes[i].box.x0, oy + h - boxes[i].box.y1,
                  ox + boxes[i].box.x1, oy + h - boxes[i].box.y0 };

    switch (boxes[i].kind)
    {
    case stageboxkind_CONTENT:
      if (ic->bitmap.base != NULL)
        screen_copy_bitmap_dithered(scr, b.x0, b.y0, &ic->bitmap,
                                    ic->dithering);
      continue;

    case stageboxkind_STROKE:
      colour = ic->colour_border;
      break;

    case stageboxkind_SHADOW:
      colour = ic->colour_shadow;
      break;

    case stageboxkind_MARGIN:
      colour = ic->colour_paper;
      break;

    default:
      colour = ic->colour_pasteboard;
      break;
    }

    screen_fill_rects(scr, &b, 1, colour);
  }
}

static result_t viewing_redraw(const wuss_event_t *event, void *task_data)
{
  viewing_task_t *ic;
  screen_t       *scr;
  const box_t    *bounds;
  int             sx, sy;
  int             bx, by;
  int             bx0, bx1, by1;
  box_t           band[4];
  const colour_t *palette;

  ic = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;

  if (ic->stage)
  {
    viewing_redraw_stage(ic, scr, bounds, bounds->x0 - sx, bounds->y0 - sy);
    return result_OK;
  }

  bx     = bounds->x0 - sx + VIEWING_MARGINSZ;
  by     = bounds->y0 - sy + VIEWING_MARGINSZ;
  bx0    = bx - VIEWING_BORDERSZ;
  bx1    = bx + ic->bitmap.size.w + VIEWING_BORDERSZ;
  by1    = by + ic->bitmap.size.h;

  /* solid 8px band around the image */
  band[0] = (box_t) { bx0, by - VIEWING_BORDERSZ, bx1, by };    /* top */
  band[1] = (box_t) { bx0, by1, bx1, by1 + VIEWING_BORDERSZ };  /* bottom */
  band[2] = (box_t) { bx0, by, bx, by1 };                     /* left */
  band[3] = (box_t) { bx + ic->bitmap.size.w, by, bx1, by1 }; /* right */
  screen_fill_rects(scr, band, 4, ic->colour_paper);

  if (ic->bitmap.base != NULL)
    screen_copy_bitmap_dithered(scr, bx, by, &ic->bitmap, ic->dithering);

  return result_OK;
}

/* Load the PNG at path and show it, resizing the window to fit. On failure
 * the current image stays and the error is returned. */
static result_t viewing_show(viewing_task_t *ic, const char *path)
{
  result_t rc;
  bitmap_t next;

  rc = load_png_deep(&next, path, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    logf_warning("viewing: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(ic->bitmap.base);
  ic->bitmap = next;

  return viewing_fit(ic);
}

/* A DataLoad on the viewing window: show the file it names. */
static result_t viewing_message(viewing_task_t     *ic,
                                wuss_window_t      *window,
                                const wuss_event_t *event)
{
  if (ic->window == NULL || window != ic->window)
    return result_OK;

  return taskapp_window_data_load(ic->wuss, ic->delegate, ic,
                                  (taskapp_data_load_fn_t) viewing_show,
                                  event->data.message);
}

/* Menu shape built from a descriptor string. The tree must outlive the open
 * chain, so it is kept on the task and rebuilt (previous one freed) on each
 * open. Freed for good in the QUIT handler. */
static result_t viewing_open_menu(viewing_task_t *ic)
{

  result_t     rc;
  wuss_menu_t *m;
  int          i;

  rc = wuss_menu_create_from_desc(&m,
         "Viewing, Dithering, Colours, !Stage", ic->stage);
  if (rc != result_OK)
    return rc;

  /* The descriptor syntax cannot name a submenu, so point "Colours" and
   * "Dithering" at per-instance submenus built at create time, by hand.
   * Marked BORROWED_SUBMENU so wuss_menu_destroy leaves them alone -- they
   * belong to the task, not to this per-open tree. */
  wuss_menu_tick_exclusive(&ic->dither_menu, (int) ic->dithering);
  for (i = 0; i < m->nitems; i++)
  {
    wuss_menu_item_t *it = (wuss_menu_item_t *) &m->items[i];

    if (it->text == NULL)
      continue;
    if (strcmp(it->text, "Dithering") == 0)
      it->submenu = &ic->dither_menu;
    else if (strcmp(it->text, "Colours") == 0)
      it->submenu = &ic->colours.menu;
    else
      continue;

    it->flags |= wuss_MENU_ITEM_BORROWED_SUBMENU;
  }

  wuss_menu_destroy(ic->menu);
  ic->menu = m;

  return wuss_menu_open_at_pointer(ic->delegate, ic->menu, &ic->menu_handle);
}

result_t viewing_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  viewing_task_t    *ic;
  const wuss_menu_t *menu;
  int                index;

  ic = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return viewing_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != ic->window)
      return result_OK; /* not the viewing window (e.g. the proginfo dialogue) */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_MENU)
      return viewing_open_menu(ic);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (wuss_colourlist_selected(&ic->colours, event))
    {
      wuss_window_invalidate_visible(ic->window);
      return result_OK;
    }

    menu  = event->data.menu_select.menu;
    index = event->data.menu_select.index;
    if (menu == ic->menu && index == VIEWING_MENU_STAGE)
    {
      ic->stage = !ic->stage;
      wuss_menu_tick_item_live(ic->menu_handle, menu, index, ic->stage);
      return viewing_fit(ic);
    }
    if (menu == &ic->dither_menu)
    {
      ic->dithering = (screen_dither_t) index;
      wuss_window_invalidate_visible(ic->window);
      wuss_menu_tick_exclusive(&ic->dither_menu, index);
      wuss_menu_tick_exclusive_live(ic->menu_handle, &ic->dither_menu, index);
    }
    return result_OK;

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    {
      result_t rc;

      if (wuss_colourlist_pre_open(&ic->colours, ic->wuss, event, &rc))
        return rc;

      return result_OK;
    }

  case wuss_EVENT_MESSAGE:
    return viewing_message(ic, window, event);

  case wuss_EVENT_MENU_CLOSED:
    ic->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == ic->window)
      ic->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    viewing_destroy(ic);
    return result_OK;

  case wuss_EVENT_KEY:
    return result_WUSS_KEY_UNCLAIMED;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
