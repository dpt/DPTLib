/* wuss/test/tasks/viewing.c -- static bitmap viewing task */

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
#include "io/path.h"
#include "wuss/menu.h"
#include "wuss/menu-desc.h"
#include "wuss/message.h"

#include "viewing.h"
#include "taskapp.h"
#include "common.h"

#define NINEPATCHSZ      9
#define VIEWING_BORDERSZ 8 /* solid inset band drawn inside the ninepatch */
#define VIEWING_MARGINSZ (NINEPATCHSZ + VIEWING_BORDERSZ)
#define VIEWING_EMPTYSZ  64 /* image area before the first DataLoad */

result_t viewing_create(wuss_t *wuss, viewing_task_t **out)
{
  result_t         rc;
  viewing_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  const char      *resources;
  const char      *background_path;
  size2d_t         sz;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss        = wuss;
  task->bitmap.size = SIZE2D(VIEWING_EMPTYSZ, VIEWING_EMPTYSZ); /* no pixels */
  task->delegate    = NULL;
  task->menu        = NULL;
  task->menu_handle = NULL;
  task->dithering   = screen_DITHER_BLUE_NOISE;

  /* indexed by screen_dither_t */
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_NONE, "None",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_BAYER, "Bayer",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_BLUE_NOISE, "Blue noise",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->dither_menu, "Dithering", task->dither_items,
                  NELEMS(task->dither_items));

  resources       = wuss_get_resources(wuss);
  background_path = pathf("%s/resources/wuss/ninepatch.png", resources);
  rc = load_png_deep(&task->ninepatch, background_path, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }

  /* shows through the image's transparent pixels */
  delegate_desc.handle    = viewing_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "viewing";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task->ninepatch.base);
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: QUIT is delivered at wuss_destroy instead. Closing the
   * main window early leaks this block until then -- fine for a demo. */

  sz.w = task->bitmap.size.w + VIEWING_MARGINSZ * 2;
  sz.h = task->bitmap.size.h + VIEWING_MARGINSZ * 2;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(sz.w, sz.h),
                                 "Viewing",
                                 wuss_WINDOW_DEFAULT,
                                 wuss_NO_BACKDROP,
                                 sz,
                                 SIZE2D(32, 32),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* QUIT frees the two bitmaps and the block */
    return rc;
  }

  if (out)
    *out = task;

  return result_OK;
}

static result_t viewing_show(viewing_task_t *ic, const char *path);

result_t viewing_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Viewing",
    "View PNG images",
    (taskapp_create_fn_t) viewing_create,
    (taskapp_data_load_fn_t) viewing_show,
    true
  };

  return taskapp_install(wuss, &desc);
}

void viewing_destroy(viewing_task_t *task)
{
  wuss_menu_destroy(task->menu);
  free(task->bitmap.base);
  free(task->ninepatch.base);
  free(task); /* task_data was calloc'd per instance by the spawner */
}

static result_t viewing_redraw(const wuss_event_t *event, void *task_data)
{
  viewing_task_t *ic;
  screen_t       *scr;
  const box_t    *bounds;
  int             sx, sy;
  int             bx, by;
  int             bx0, bx1, by1;
  box_t           behind;
  box_t           band[4];
  const colour_t *palette;

  ic = task_data;

  scr    = event->data.redraw.scr;
  bounds = event->data.redraw.bounds;
  sx     = event->data.redraw.scroll.x;
  sy     = event->data.redraw.scroll.y;
  bx     = bounds->x0 - sx + VIEWING_MARGINSZ;
  by     = bounds->y0 - sy + VIEWING_MARGINSZ;
  bx0    = bx - VIEWING_BORDERSZ;
  bx1    = bx + ic->bitmap.size.w + VIEWING_BORDERSZ;
  by1    = by + ic->bitmap.size.h;

  behind.x0 = bx - VIEWING_MARGINSZ;
  behind.y0 = by - VIEWING_MARGINSZ;
  behind.x1 = behind.x0 + ic->bitmap.size.w + VIEWING_MARGINSZ * 2;
  behind.y1 = behind.y0 + ic->bitmap.size.h + VIEWING_MARGINSZ * 2;
  screen_copy_ninepatch(scr, &behind, &ic->ninepatch, screen_NINEPATCH_NO_CENTRE);

  /* solid 8px band between the ninepatch frame and the image */
  band[0] = (box_t) { bx0, by - VIEWING_BORDERSZ, bx1, by };    /* top */
  band[1] = (box_t) { bx0, by1, bx1, by1 + VIEWING_BORDERSZ };  /* bottom */
  band[2] = (box_t) { bx0, by, bx, by1 };                     /* left */
  band[3] = (box_t) { bx + ic->bitmap.size.w, by, bx1, by1 }; /* right */
  palette = wuss_get_palette(ic->wuss, NULL);
  screen_fill_rects(scr, band, 4, palette[ic->background]);

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
  size2d_t sz;

  rc = load_png_deep(&next, path, pixelfmt_rgbx8888);
  if (rc != result_OK)
  {
    logf_warning("viewing: cannot load \"%s\" (rc=0x%X)", path, rc);
    return rc;
  }

  free(ic->bitmap.base);
  ic->bitmap = next;

  sz.w = ic->bitmap.size.w + VIEWING_MARGINSZ * 2;
  sz.h = ic->bitmap.size.h + VIEWING_MARGINSZ * 2;

  rc = wuss_window_resize(ic->window, sz);
  if (rc != result_OK)
    return rc;

  return wuss_window_set_doc(ic->window, sz);
}

/* A DataLoad on the viewing window: show the file it names. A file that will
 * not load is not acknowledged, so the send bounces to the sender. */
static result_t viewing_message(viewing_task_t     *ic,
                                wuss_window_t      *window,
                                const wuss_event_t *event)
{
  const wuss_message_t *msg;
  char                  path[DPTLIB_MAXPATH];
  size_t                len;

  msg = event->data.message;

  if (msg->action != wuss_MESSAGE_DATA_LOAD || ic->window == NULL ||
      window != ic->window)
    return result_OK;

  len = MIN(msg->size, sizeof(path) - 1);
  memcpy(path, msg->data, len);
  path[len] = '\0';

  if (viewing_show(ic, path) != result_OK)
    return result_OK;

  (void) wuss_acknowledge(ic->wuss, ic->delegate, msg);
  return result_OK;
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
         "Viewing, Dithering, Background");
  if (rc != result_OK)
    return rc;

  /* The descriptor syntax has no "open this submenu on hover" mark, so point
   * "Background" at the shared colourmenu singleton by hand as a submenu, so
   * it gets the usual arrow-and-hover behaviour.
   * Marked BORROWED_SUBMENU so wuss_menu_destroy leaves it alone -- it is
   * owned by the singleton, not by this per-open tree. No "None" row: its
   * wuss_NO_BACKGROUND isn't a palette index, and there's nothing behind
   * the band to show through anyway. */
  wuss_colourmenu_set_none(0);
  for (i = 0; i < m->nitems; i++)
    if (m->items[i].text != NULL && strcmp(m->items[i].text, "Background") == 0)
    {
      wuss_menu_item_t *it = (wuss_menu_item_t *) &m->items[i];

      it->submenu = wuss_colourmenu_menu(ic->wuss);
      it->flags  |= wuss_MENU_ITEM_BORROWED_SUBMENU;
      wuss_colourmenu_set_title("Background"); /* needs it built first */
      wuss_colourmenu_set_ticked(ic->background);
      break;
    }

  /* And "Dithering": a per-instance submenu built at create time and ticked
   * here. Borrowed for the same reason. */
  wuss_menu_tick_exclusive(&ic->dither_menu, (int) ic->dithering);
  for (i = 0; i < m->nitems; i++)
  {
    wuss_menu_item_t *it = (wuss_menu_item_t *) &m->items[i];

    if (it->text == NULL)
      continue;
    if (strcmp(it->text, "Dithering") != 0)
      continue;

    it->submenu = &ic->dither_menu;
    it->flags  |= wuss_MENU_ITEM_BORROWED_SUBMENU;
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
    {
      wuss_colour_t picked;
      int           mine;

      picked = wuss_colourmenu_selected(event, &mine);
      if (mine)
      {
        ic->background = picked;
        wuss_window_invalidate_visible(ic->window);
        return result_OK;
      }
    }

    menu  = event->data.menu_select.menu;
    index = event->data.menu_select.index;
    if (menu == &ic->dither_menu)
    {
      ic->dithering = (screen_dither_t) index;
      wuss_window_invalidate_visible(ic->window);
      wuss_menu_tick_exclusive(&ic->dither_menu, index);
      wuss_menu_tick_exclusive_live(ic->menu_handle, &ic->dither_menu, index);
    }
    return result_OK;

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

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
