/* wuss/test/tasks/display.c -- desktop display mode picker task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "geom/size.h"
#include "geom/stack.h"
#include "wuss/icon-spec.h"
#include "wuss/icon.h"

#include "tasks.h" /* app_set_mode, app_get_depth */

#include "display.h"

enum { DISPLAY_MENU_INFO };

#define DISPLAY_LABEL_W      66 /* px; enough for "Resolution" at 6px/char */
#define DISPLAY_SET_W        120
#define DISPLAY_ROW_H        18
#define DISPLAY_CHAR_W       6 /* ponytail: assumes the 6px system font */
#define DISPLAY_ACTION_W(W)  (((W) + 2) * DISPLAY_CHAR_W + \
                              2 * wuss_STD_SECONDARY_BUTTON_BORDER)
#define DISPLAY_DEFAULT_W(W) (((W) + 2) * DISPLAY_CHAR_W + \
                              2 * wuss_STD_PRIMARY_BUTTON_BORDER)

enum
{
  DISPLAY_ST_ROOT,
  DISPLAY_ST_COLOURS_ROW,
  DISPLAY_ST_COLOURS_LABEL,
  DISPLAY_ST_COLOURS,
  DISPLAY_ST_RESOLUTION_ROW,
  DISPLAY_ST_RESOLUTION_LABEL,
  DISPLAY_ST_RESOLUTION,
  DISPLAY_ST_BUTTONS,
  DISPLAY_ST_BUTTONS_SPACER,
  DISPLAY_ST_CANCEL,
  DISPLAY_ST_CHANGE,
  DISPLAY_ST__LIMIT
};

static const stack_item_t g_display_stack[DISPLAY_ST__LIMIT] =
{
  [DISPLAY_ST_ROOT] = { .kind = stack_KIND_VBOX, .parent = -1,
                        .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },

  [DISPLAY_ST_COLOURS_ROW]      = STACK_HBOX(DISPLAY_ST_ROOT, DISPLAY_ROW_H, wuss_STD_GAP, stack_ALIGN_START),
  [DISPLAY_ST_COLOURS_LABEL]    = STACK_LEAF(DISPLAY_ST_COLOURS_ROW, DISPLAY_LABEL_W, DISPLAY_ROW_H, stack_ALIGN_CENTRE),
  [DISPLAY_ST_COLOURS]          = STACK_LEAF(DISPLAY_ST_COLOURS_ROW, DISPLAY_SET_W, DISPLAY_ROW_H, stack_ALIGN_CENTRE),

  [DISPLAY_ST_RESOLUTION_ROW]   = STACK_HBOX(DISPLAY_ST_ROOT, DISPLAY_ROW_H, wuss_STD_GAP, stack_ALIGN_START),
  [DISPLAY_ST_RESOLUTION_LABEL] = STACK_LEAF(DISPLAY_ST_RESOLUTION_ROW, DISPLAY_LABEL_W, DISPLAY_ROW_H, stack_ALIGN_CENTRE),
  [DISPLAY_ST_RESOLUTION]       = STACK_LEAF(DISPLAY_ST_RESOLUTION_ROW, DISPLAY_SET_W, DISPLAY_ROW_H, stack_ALIGN_CENTRE),

  [DISPLAY_ST_BUTTONS]          = STACK_HBOX(DISPLAY_ST_ROOT, wuss_STD_PRIMARY_BUTTON_HEIGHT, wuss_STD_GAP, stack_ALIGN_END),
  [DISPLAY_ST_BUTTONS_SPACER]   = STACK_SPACER(DISPLAY_ST_BUTTONS, 1), /* push the buttons right */
  [DISPLAY_ST_CANCEL]           = STACK_LEAF(DISPLAY_ST_BUTTONS, DISPLAY_ACTION_W(6), wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
  [DISPLAY_ST_CHANGE]           = STACK_LEAF(DISPLAY_ST_BUTTONS, DISPLAY_DEFAULT_W(6), wuss_STD_PRIMARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
};

enum
{
  DISPLAY_ICON_COLOURS_LABEL,
  DISPLAY_ICON_RESOLUTION_LABEL,
  DISPLAY_ICON_CANCEL,
  DISPLAY_ICON_CHANGE,
  DISPLAY_NICONS
};

/* fixed set of depths the Colours picker offers, in bits per pixel;
 * app_set_mode rejects (via wuss_frontend_resize) anything a backend can't
 * honour, e.g. RISC OS's fixed screen mode */
static const int g_display_depths[] =
{
  1, 2, 4, 8, 32
};

/* wuss_stringset_create borrows its strings, so these are static, paired
 * 1:1 with g_display_depths */
static const char *const g_display_depth_labels[NELEMS(g_display_depths)] =
{
  "2 colours",
  "4 colours",
  "16 colours",
  "256 colours",
  "16M colours"
};

/* fixed set of resolutions the Resolution picker offers; rejected likewise */
static const size2d_t g_display_resolutions[] =
{
  { 480,  360  },
  { 640,  480  },
  { 800,  600  },
  { 1024, 768  },
  { 1152, 864  },
  { 1280, 720  },
  { 1280, 800  },
  { 1366, 768  },
  { 1920, 1080 }
};

/* borrowed likewise, paired 1:1 with g_display_resolutions */
static const char *const g_display_resolution_labels[NELEMS(g_display_resolutions)] =
{
  "480 x 360",
  "640 x 480",
  "800 x 600",
  "1024 x 768",
  "1152 x 864",
  "1280 x 720",
  "1280 x 800",
  "1366 x 768",
  "1920 x 1080"
};

/* ----------------------------------------------------------------------- */

/* the entry matching the current depth. ponytail: an unlisted mode shows
 * entry 0 */
static int display_depth_index(void)
{
  int depth;
  int i;

  depth = app_get_depth();
  for (i = 0; i < (int) NELEMS(g_display_depths); i++)
    if (g_display_depths[i] == depth)
      return i;

  return 0;
}

/* the entry matching the current screen size; unlisted likewise */
static int display_resolution_index(const wuss_t *wuss)
{
  size2d_t size;
  int      i;

  size = wuss_get_screen_size(wuss);
  for (i = 0; i < (int) NELEMS(g_display_resolutions); i++)
    if (g_display_resolutions[i].w == size.w &&
        g_display_resolutions[i].h == size.h)
      return i;

  return 0;
}

/* show the mode actually in force, e.g. after app_set_mode refused a pick */
static void display_sync(display_task_t *dc)
{
  (void) wuss_stringset_set_index(dc->colours, display_depth_index());
  (void) wuss_stringset_set_index(dc->resolution,
                                  display_resolution_index(dc->wuss));
}

/* apply the picked mode; on refusal show the one still in force */
static result_t display_change(display_task_t *dc)
{
  result_t rc;
  int      res;
  int      depth;

  res   = wuss_stringset_get_index(dc->resolution);
  depth = wuss_stringset_get_index(dc->colours);
  rc    = app_set_mode(g_display_resolutions[res], g_display_depths[depth]);
  if (rc != result_OK)
    display_sync(dc);

  return rc;
}

/* ----------------------------------------------------------------------- */

static result_t display_create_window(display_task_t *task,
                                      wuss_task_t    *delegate)
{
  result_t         rc;
  size2d_t         size;
  box_t            root;
  box_t            boxes[DISPLAY_ST__LIMIT];
  wuss_icon_spec_t specs[DISPLAY_NICONS];
  wuss_icon_t     *made[DISPLAY_NICONS];

  rc = stack_smallest(g_display_stack, NELEMS(g_display_stack), &size);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, size.w, size.h);
  rc = stack_solve(g_display_stack, NELEMS(g_display_stack), &root, boxes);
  if (rc != result_OK)
    return rc;

  rc = wuss_window_create_placed(delegate,
                                 size,
                                 "Display",
                                 wuss_WINDOW_CLOSE | wuss_WINDOW_BACK,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 size,
                                 SIZE2D(0, 0),
                                 &task->window);
  if (rc != result_OK)
    return rc;

  wuss_icon_spec_label(&specs[DISPLAY_ICON_COLOURS_LABEL],
                       boxes[DISPLAY_ST_COLOURS_LABEL],
                       "Colours", wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_label(&specs[DISPLAY_ICON_RESOLUTION_LABEL],
                       boxes[DISPLAY_ST_RESOLUTION_LABEL],
                       "Resolution", wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_action(&specs[DISPLAY_ICON_CANCEL],
                        boxes[DISPLAY_ST_CANCEL], "Cancel", 0);
  wuss_icon_spec_action(&specs[DISPLAY_ICON_CHANGE],
                        boxes[DISPLAY_ST_CHANGE], "Change", 1);

  rc = wuss_icon_create_array(task->window, specs, NELEMS(specs), made);
  if (rc != result_OK)
    return rc;

  task->cancel = made[DISPLAY_ICON_CANCEL];
  task->change = made[DISPLAY_ICON_CHANGE];

  /* picks only update the fields; Change applies them */
  rc = wuss_stringset_create(&task->colours, task->window,
                             boxes[DISPLAY_ST_COLOURS], "Colours",
                             g_display_depth_labels,
                             NELEMS(g_display_depth_labels), NULL, NULL);
  if (rc != result_OK)
    return rc;

  rc = wuss_stringset_create(&task->resolution, task->window,
                             boxes[DISPLAY_ST_RESOLUTION], "Resolution",
                             g_display_resolution_labels,
                             NELEMS(g_display_resolution_labels), NULL, NULL);
  if (rc != result_OK)
    return rc;

  display_sync(task);

  return result_OK;
}

result_t display_create(wuss_t *wuss, display_task_t **out)
{
  result_t         rc;
  display_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  delegate_desc.handle    = display_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "display";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = display_create_window(task, delegate);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block
                                  * and any string sets made so far */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, DISPLAY_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * display_mouse */

  WUSS_MENU_TITLE(task->menu, "Display", task->menu_items,
                 NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void display_destroy(display_task_t *task)
{
  wuss_stringset_destroy(task->colours);
  wuss_stringset_destroy(task->resolution);
  wuss_menu_close(task->menu_handle);
  free(task);
}

static result_t display_mouse(display_task_t *dc, wuss_button_t button)
{
  if (!(button & wuss_BUTTON_MENU))
    return result_OK;

  {
    static const wuss_proginfo_desc_t desc =
    {
      "Display",
      "Change the desktop colours and resolution",
      "© DPTLib contributors",
      "1.0 (" __DATE__ ")"
    };
    wuss_proginfo_set_desc(&desc);
    dc->menu_items[DISPLAY_MENU_INFO].window = wuss_proginfo_window(dc->delegate);
  }

  return wuss_menu_open(dc->delegate, &dc->menu, wuss_get_pointer(dc->wuss),
                        &dc->menu_handle);
}

/* offer an event to both string sets; 1 if one consumed it, its outcome in
 * *rc */
static int display_offer(display_task_t     *dc,
                         const wuss_event_t *event,
                         result_t           *rc)
{
  return wuss_stringset_handle_event(dc->colours, event, rc) ||
         wuss_stringset_handle_event(dc->resolution, event, rc);
}

result_t display_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  result_t        rc;
  display_task_t *dc;

  dc = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    if (display_offer(dc, event, &rc))
      return rc;

    if (event->data.icon.action != wuss_MOUSE_UP)
      return result_OK;

    if (event->data.icon.icon == dc->change)
    {
      rc = display_change(dc);
      if (rc != result_OK || (event->data.icon.button & wuss_BUTTON_ADJUST))
        return rc;

      /* Select closes, as on RISC OS; with autoclose this frees dc, so it
       * must not be touched afterwards */
      return wuss_window_try_close(dc->window);
    }
    if (event->data.icon.icon == dc->cancel)
    {
      if (!(event->data.icon.button & wuss_BUTTON_ADJUST))
        return wuss_window_try_close(dc->window); /* frees dc likewise */

      display_sync(dc); /* revert the fields to the mode in force */
    }
    return result_OK;

  case wuss_EVENT_MOUSE:
    if (window != dc->window)
      return result_OK; /* the proginfo dialogue has no click behaviour of
                         * its own */
    if (event->data.mouse.action != wuss_MOUSE_DOWN)
      return result_OK;
    return display_mouse(dc, event->data.mouse.button);

  case wuss_EVENT_MENU_SELECT:
    if (display_offer(dc, event, &rc))
      return rc;

    return result_OK; /* Info is a submenu only; nothing to act on */

  case wuss_EVENT_MENU_CLOSED:
    /* both sets must see it, and neither consumes it */
    (void) wuss_stringset_handle_event(dc->colours, event, &rc);
    (void) wuss_stringset_handle_event(dc->resolution, event, &rc);
    dc->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
    if (window == dc->menu_items[DISPLAY_MENU_INFO].window)
      rc = wuss_proginfo_handle_pre_show();
    else
      rc = result_OK;
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);

  case wuss_EVENT_CLOSE:
    if (window == dc->window)
      dc->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    display_destroy(dc);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
