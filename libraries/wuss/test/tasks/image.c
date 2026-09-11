/* wuss/test/tasks/image.c -- static bitmap image task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/palettes.h"
#include "geom/box.h"
#include "io/namelist.h"
#include "io/path.h"
#include "wuss/menu.h"
#include "wuss/menu-desc.h"

#include "image.h"

#define NINEPATCHSZ    9
#define IMAGE_BORDERSZ 8 /* solid inset band drawn inside the ninepatch */
#define IMAGE_MARGINSZ (NINEPATCHSZ + IMAGE_BORDERSZ)
#define IMAGE_EXT      ".png"

/* screen_copy_bitmap and screen_copy_ninepatch only understand a deep 32bpp
 * source in R,G,B,A/X byte order (see bitmap_load_png()); bitmap_load_png
 * keeps a palette-type PNG as pixelfmt_p8, so convert one back to rgbx8888
 * here rather than teach every blitter a paletted source. rgbx8888, not
 * bgrx8888: the latter is BGR (SDL display byte order) and would swap red
 * and blue once the blitters re-read it as rgba8888. */
static result_t load_png_deep(bitmap_t *bm, const char *filename)
{
  result_t  rc;
  bitmap_t *deep;

  rc = bitmap_load_png(bm, filename);
  if (rc != result_OK)
    return rc;

  if (bm->format != pixelfmt_p8)
    return result_OK;

  rc = bitmap_convert(bm, pixelfmt_rgbx8888, &deep);
  if (rc != result_OK)
  {
    free(bm->base);
    return rc;
  }

  free(bm->base);
  free(bm->palette);
  *bm = *deep;
  free(deep);

  return result_OK;
}

result_t image_create(wuss_t       *wuss,
                      const char   *resources,
                      const char   *path,
                      const char   *background_path,
                      image_task_t *task)
{
  result_t         rc;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  const char      *images_dir;
  size2d_t         sz;

  task->wuss        = wuss;
  task->delegate    = NULL;
  task->resources   = resources;
  task->index       = 0;
  task->nnames      = 0;
  task->menu        = NULL;
  task->proginfo    = NULL;
  task->colourmenu  = NULL;
  task->menu_handle = NULL;
  task->dithering   = 1;

  images_dir = path_join_filename(resources, 2, "resources", "images");
  rc = namelist_scan(images_dir, IMAGE_EXT, task->names[0],
                     sizeof(task->names[0]), IMAGE_MAX_NAMES, 0 /* unsorted */,
                     &task->nnames);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = load_png_deep(&task->bitmap, path);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  rc = load_png_deep(&task->ninepatch, background_path);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }

  /* shows through the image's transparent pixels */
  delegate_desc.handle    = image_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "image";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task->bitmap.base);
    free(task->ninepatch.base);
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  task->delegate = delegate;
  /* No autoclose: the task also owns the hidden proginfo window below, so its
   * window list never empties while the main window is up. QUIT is delivered
   * at wuss_destroy instead. Closing the main window early leaks this block
   * until then -- fine for a demo. */

  sz.w = task->bitmap.size.w + IMAGE_MARGINSZ * 2;
  sz.h = task->bitmap.size.h + IMAGE_MARGINSZ * 2;

  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(sz.w, sz.h),
                                 "Image",
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

  /* The "Info" menu row's standard dialogue. Hung off the descriptor menu as
   * a wuss_menu_item_t.window in image_open_menu. A create failure is
   * non-fatal -- the task just runs without an Info dialogue. */
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Image",
      "View the PNGs under resources/images",
      "(c) DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };

    if (wuss_proginfo_create(&task->proginfo, delegate, &desc) != result_OK)
      task->proginfo = NULL;
  }

  /* The "Background" menu row's submenu. Hung off the descriptor menu as a
   * wuss_menu_item_t.submenu in image_open_menu. A create failure is
   * non-fatal -- the task just runs without a Background submenu. */
  if (wuss_colourmenu_create(&task->colourmenu, wuss, "Background") != result_OK)
    task->colourmenu = NULL;

  return result_OK;
}

static result_t image_redraw(const wuss_event_t *event, void *task_data)
{
  image_task_t   *ic;
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
  bx     = bounds->x0 - sx + IMAGE_MARGINSZ;
  by     = bounds->y0 - sy + IMAGE_MARGINSZ;
  bx0    = bx - IMAGE_BORDERSZ;
  bx1    = bx + ic->bitmap.size.w + IMAGE_BORDERSZ;
  by1    = by + ic->bitmap.size.h;

  behind.x0 = bx - IMAGE_MARGINSZ;
  behind.y0 = by - IMAGE_MARGINSZ;
  behind.x1 = behind.x0 + ic->bitmap.size.w + IMAGE_MARGINSZ * 2;
  behind.y1 = behind.y0 + ic->bitmap.size.h + IMAGE_MARGINSZ * 2;
  screen_copy_ninepatch(scr, &behind, &ic->ninepatch, screen_NINEPATCH_NO_CENTRE);

  /* solid 8px band between the ninepatch frame and the image */
  band[0] = (box_t) { bx0, by - IMAGE_BORDERSZ, bx1, by };    /* top */
  band[1] = (box_t) { bx0, by1, bx1, by1 + IMAGE_BORDERSZ };  /* bottom */
  band[2] = (box_t) { bx0, by, bx, by1 };                     /* left */
  band[3] = (box_t) { bx + ic->bitmap.size.w, by, bx1, by1 }; /* right */
  palette = wuss_get_palette(ic->wuss, NULL);
  screen_fill_rects(scr, band, 4, palette[ic->background]);

  if (ic->dithering)
    screen_copy_bitmap_dithered(scr, bx, by, &ic->bitmap);
  else
    screen_copy_bitmap(scr, bx, by, &ic->bitmap);

  return result_OK;
}

static result_t image_click(wuss_window_t *window,
                            image_task_t  *ic,
                            int            step)
{
  result_t    rc;
  const char *leafname;
  const char *filename;
  char        buf[DPTLIB_MAXPATH];
  bitmap_t    next;
  size2d_t    sz;

  if (ic->nnames == 0)
    return result_OK; /* nothing to cycle to */

  ic->index = (ic->index + step + ic->nnames) % ic->nnames;

  leafname = path_join_leafname(ic->names[ic->index], "png");
  filename = path_join_filename(ic->resources, 3, "resources", "images",
                                leafname);
  strcpy(buf, filename);

  rc = load_png_deep(&next, buf);
  if (rc != result_OK)
  {
    logf_warning("image: skipping \"%s\" (rc=0x%X)", buf, rc);
    return result_OK; /* keep showing the current image */
  }

  free(ic->bitmap.base);
  ic->bitmap = next;

  sz.w = ic->bitmap.size.w + IMAGE_MARGINSZ * 2;
  sz.h = ic->bitmap.size.h + IMAGE_MARGINSZ * 2;

  rc = wuss_window_resize(window, sz);
  if (rc != result_OK)
    return rc;
  return wuss_window_set_doc(window, sz);
}

/* Same menu shape built from a descriptor string, to exercise
 * wuss_menu_create_from_desc. The tree must outlive the open chain, so it is
 * kept on the task and rebuilt (previous one freed) on each open. Freed for
 * good in the QUIT handler. */
static const wuss_menu_item_t image_menu_export_items[] =
{
  { "As PNG",  wuss_MENU_ITEM_NONE,     NULL },
  { "As JPEG", wuss_MENU_ITEM_NONE,     NULL },
  { "As GIF",  wuss_MENU_ITEM_DISABLED, NULL }
};

static const wuss_menu_t image_menu_export =
{
  "Export", image_menu_export_items, NELEMS(image_menu_export_items)
};

static result_t image_open_menu(image_task_t *ic)
{
  result_t     rc;
  wuss_menu_t *m;
  int          i;

  /* '!Dithering' pulls ic->dithering directly, so the row's tick already
   * matches live state -- no separate wuss_menu_open_ticked pass needed.
   * '!Wireframe' has no backing field; it is a demo row always ticked. */
  rc = wuss_menu_create_from_desc(&m,
         "Image, Info, New..., Open, !Dithering, !Wireframe, >Export, "
         "Background, |Quit",
         ic->dithering, 1, &image_menu_export);
  if (rc != result_OK)
    return rc;

  /* The descriptor syntax has no "open this window on hover" mark, so point
   * the "Info" row at the proginfo dialogue by hand: wuss treats a
   * wuss_menu_item_t.window exactly like a submenu, showing it where one would
   * open. */
  if (ic->proginfo != NULL)
    for (i = 0; i < m->nitems; i++)
      if (m->items[i].text != NULL && strcmp(m->items[i].text, "Info") == 0)
      {
        ((wuss_menu_item_t *) m->items)[i].window =
          wuss_proginfo_window(ic->proginfo);
        break;
      }

  /* Same trick for "Background": point it at the colourmenu's own menu tree
   * as a submenu, so it gets the usual arrow-and-hover behaviour. Marked
   * BORROWED_SUBMENU so wuss_menu_destroy leaves it alone -- it is owned by
   * ic->colourmenu, built once at task creation and reused on every open,
   * not by this per-open tree. */
  if (ic->colourmenu != NULL)
    for (i = 0; i < m->nitems; i++)
      if (m->items[i].text != NULL && strcmp(m->items[i].text, "Background") == 0)
      {
        wuss_menu_item_t *it = (wuss_menu_item_t *) &m->items[i];

        it->submenu = wuss_colourmenu_menu(ic->colourmenu);
        it->flags  |= wuss_MENU_ITEM_BORROWED_SUBMENU;
        break;
      }

  wuss_menu_destroy(ic->menu);
  ic->menu = m;

  return wuss_menu_open(ic->delegate,
                        ic->menu,
                        wuss_get_pointer(ic->wuss),
                       &ic->menu_handle);
}

result_t image_handle(wuss_window_t      *window,
                      const wuss_event_t *event,
                      void               *task_data)
{
  image_task_t      *ic;
  const wuss_menu_t *menu;
  int                index;

  ic = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return image_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    if (window != ic->window)
      return result_OK; /* not the image window (e.g. the proginfo dialogue) */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    if (event->data.mouse.button & wuss_BUTTON_SELECT)
      return image_click(window, ic, 1);
    if (event->data.mouse.button & wuss_BUTTON_ADJUST)
      return image_click(window, ic, -1);
    if (event->data.mouse.button & wuss_BUTTON_MENU)
      return image_open_menu(ic);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    {
      wuss_colour_t picked;
      int           mine;

      picked = wuss_colourmenu_selected(ic->colourmenu, event, &mine);
      if (mine)
      {
        ic->background = picked;
        wuss_window_invalidate_visible(ic->window);
        if (!wuss_menu_should_keep_open(event))
          ic->menu_handle = NULL; /* SELECT pick already freed the chain */
        return result_OK;
      }
    }

    menu  = event->data.menu_select.menu;
    index = event->data.menu_select.index;
    printf("image menu: picked \"%s\"\n",
           menu->items[index].text ? menu->items[index].text : "(sep)");
    if (index == 3) {
      ic->dithering = !ic->dithering;
      wuss_window_invalidate_visible(ic->window);
    }
    if (!wuss_menu_should_keep_open(event))
      ic->menu_handle = NULL; /* SELECT pick already freed the chain */
    else if (index == 3)
      wuss_menu_tick_item_live(ic->menu_handle, ic->menu, 3, ic->dithering);
    return result_OK;

  case wuss_EVENT_MENU_CLOSED:
    ic->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_QUIT:
    /* close any open chain first: it may hold the proginfo window as a
     * borrowed wuss_menu_item_t.window, and destroying that below would leave
     * the chain pointing at freed memory */
    wuss_menu_close(ic->menu_handle);
    wuss_menu_destroy(ic->menu);
    wuss_proginfo_destroy(ic->proginfo); /* closes its dialogue window */
    wuss_colourmenu_destroy(ic->colourmenu);
    free(ic->bitmap.base);
    free(ic->ninepatch.base);
    free(ic); /* task_data was calloc'd per instance by the spawner */
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
