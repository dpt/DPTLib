/* wuss/test/tasks/paint-new.c -- paint's New dialogue */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "geom/stack.h"
#include "wuss/component/dialogue.h"
#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "common.h"
#include "paint-new.h"

/* ----------------------------------------------------------------------- */

#define PN_FIELD_H  22
#define PN_FIELD_W  60
#define PN_DIGITS   4    /* PAINTNEW_SIZE_MAX is four digits */
#define PN_G_LABEL  1    /* size group, see stack_item_t::group */

#define PN_BAR_GAP   47 /* the gap an icon bar menu leaves */

#define PN_DEFAULT_W 640
#define PN_DEFAULT_H 480

/* the background choices, in the order of the radios */
enum
{
  PN_BG_WHITE,
  PN_BG_BLACK,
  PN_BG_GREY,
  PN_BG__COUNT
};

static const char *const paintnew_bg_names[PN_BG__COUNT] =
{
  "White", "Black", "Grey"
};

/* icons, in creation order */
enum
{
  PN_ICON_WIDTH_LABEL,
  PN_ICON_WIDTH,
  PN_ICON_HEIGHT_LABEL,
  PN_ICON_HEIGHT,
  PN_ICON_BG_LABEL,
  PN_ICON_BG,        /* first of PN_BG__COUNT radios */
  PN_ICON_CANCEL = PN_ICON_BG + PN_BG__COUNT,
  PN_ICON_CREATE,
  PN_ICON__COUNT
};

/* layout items, in creation order. See docs/windowing/wuss-layout.md */
enum
{
  PN_ST_ROOT,
  PN_ST_WIDTH_ROW,
  PN_ST_WIDTH_LABEL,
  PN_ST_WIDTH,
  PN_ST_HEIGHT_ROW,
  PN_ST_HEIGHT_LABEL,
  PN_ST_HEIGHT,
  PN_ST_BG_ROW,
  PN_ST_BG_LABEL,
  PN_ST_BG,          /* first of PN_BG__COUNT radios */
  PN_ST_BUTTONS = PN_ST_BG + PN_BG__COUNT,
  PN_ST_SPACER,
  PN_ST_CANCEL,
  PN_ST_CREATE,
  PN_ST__LIMIT
};

#define PN_ST_RADIO(i) \
  [PN_ST_BG + (i)] = STACK_LEAF(PN_ST_BG_ROW, 0, wuss_STD_OPTION_SIZE, stack_ALIGN_CENTRE)

static const stack_item_t g_paintnew_stack[PN_ST__LIMIT] =
{
  [PN_ST_ROOT] = { .kind = stack_KIND_VBOX, .parent = -1,
                   .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },

  [PN_ST_WIDTH_ROW]    = STACK_HBOX(PN_ST_ROOT, PN_FIELD_H, wuss_STD_GAP, stack_ALIGN_START),
  [PN_ST_WIDTH_LABEL]  = STACK_LEAF_GROUP(PN_ST_WIDTH_ROW, 0, PN_FIELD_H, stack_ALIGN_CENTRE, PN_G_LABEL),
  [PN_ST_WIDTH]        = STACK_LEAF(PN_ST_WIDTH_ROW, PN_FIELD_W, PN_FIELD_H, stack_ALIGN_CENTRE),

  [PN_ST_HEIGHT_ROW]   = STACK_HBOX(PN_ST_ROOT, PN_FIELD_H, wuss_STD_GAP, stack_ALIGN_START),
  [PN_ST_HEIGHT_LABEL] = STACK_LEAF_GROUP(PN_ST_HEIGHT_ROW, 0, PN_FIELD_H, stack_ALIGN_CENTRE, PN_G_LABEL),
  [PN_ST_HEIGHT]       = STACK_LEAF(PN_ST_HEIGHT_ROW, PN_FIELD_W, PN_FIELD_H, stack_ALIGN_CENTRE),

  [PN_ST_BG_ROW]       = STACK_HBOX(PN_ST_ROOT, wuss_STD_OPTION_SIZE, wuss_STD_GAP, stack_ALIGN_START),
  [PN_ST_BG_LABEL]     = STACK_LEAF_GROUP(PN_ST_BG_ROW, 0, wuss_STD_OPTION_SIZE, stack_ALIGN_CENTRE, PN_G_LABEL),
  PN_ST_RADIO(0),
  PN_ST_RADIO(1),
  PN_ST_RADIO(2),

  [PN_ST_BUTTONS]      = STACK_HBOX(PN_ST_ROOT, wuss_STD_PRIMARY_BUTTON_HEIGHT, wuss_STD_GAP, stack_ALIGN_END),
  [PN_ST_SPACER]       = STACK_SPACER(PN_ST_BUTTONS, 1), /* push the buttons right */
  [PN_ST_CANCEL]       = STACK_LEAF(PN_ST_BUTTONS, 0, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
  [PN_ST_CREATE]       = STACK_LEAF(PN_ST_BUTTONS, 0, wuss_STD_PRIMARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE),
};

struct paintnew
{
  paintnew_t           **slot;     /* cleared on QUIT */
  wuss_t                *wuss;
  wuss_task_t           *task;
  wuss_dialogue_t       *dialogue;
  paintnew_accept_fn_t  *accept;
  void                  *opaque;
  wuss_menu_handle_t     menu_handle; /* live only while shown */
  wuss_icon_t           *icons[PN_ICON__COUNT];
};

/* ----------------------------------------------------------------------- */

/* the colours for a background choice: the canvas, and a pen that shows on it */
static void paintnew_colours(int which, paintnew_params_t *params)
{
  switch (which)
  {
  case PN_BG_BLACK:
    params->bg = colour_rgb(0x00, 0x00, 0x00);
    params->fg = colour_rgb(0xFF, 0xFF, 0xFF);
    break;

  case PN_BG_GREY:
    params->bg = colour_rgb(0xC0, 0xC0, 0xC0);
    params->fg = colour_rgb(0x00, 0x00, 0x00);
    break;

  default:
    params->bg = colour_rgb(0xFF, 0xFF, 0xFF);
    params->fg = colour_rgb(0x00, 0x00, 0x00);
    break;
  }
}

/* the number in a size field, clamped to the allowed range, with the field
 * rewritten to match. Anything that is not a number reads as the minimum */
static int paintnew_read_size(const paintnew_t *pn, int which)
{
  wuss_icon_t *icon;
  int          size;
  char         text[PN_DIGITS + 2];

  icon = pn->icons[which];
  size = (int) strtol(wuss_icon_get_text(icon), NULL, 10);
  size = CLAMP(size, PAINTNEW_SIZE_MIN, PAINTNEW_SIZE_MAX);

  snprintf(text, sizeof(text), "%d", size);
  (void) wuss_icon_set_text(wuss_dialogue_window(pn->dialogue), icon, text);

  return size;
}

static int paintnew_read_bg(const paintnew_t *pn)
{
  int i;

  for (i = 0; i < PN_BG__COUNT; i++)
    if (wuss_icon_get_selected(pn->icons[PN_ICON_BG + i]))
      return i;

  return PN_BG_WHITE;
}

static void paintnew_dismiss(paintnew_t *pn)
{
  wuss_menu_close(pn->menu_handle);
  pn->menu_handle = NULL;
}

static result_t paintnew_action_cancel(void *opaque, wuss_button_t button)
{
  paintnew_t *pn;

  pn = opaque;

  /* ADJUST leaves the dialogue open, as on RISC OS */
  if (!(button & wuss_BUTTON_ADJUST))
    paintnew_dismiss(pn);

  return result_OK;
}

static result_t paintnew_action_create(void *opaque, wuss_button_t button)
{
  paintnew_t       *pn;
  paintnew_params_t params;

  pn = opaque;

  params.size.w = paintnew_read_size(pn, PN_ICON_WIDTH);
  params.size.h = paintnew_read_size(pn, PN_ICON_HEIGHT);
  paintnew_colours(paintnew_read_bg(pn), &params);

  if (!(button & wuss_BUTTON_ADJUST))
    paintnew_dismiss(pn);

  return pn->accept(&params, pn->opaque);
}

/* ----------------------------------------------------------------------- */

/* solve the layout: the window size, and a box for every item */
static result_t paintnew_layout(const wuss_t *wuss,
                                size2d_t     *size,
                                box_t         boxes[PN_ST__LIMIT])
{
  result_t     rc;
  stack_item_t items[PN_ST__LIMIT];
  box_t        root;
  int          i;

  /* each label's natural width; the size group widens the others to match */
  memcpy(items, g_paintnew_stack, sizeof(items));
  items[PN_ST_WIDTH_LABEL].axis_size  = task_text_width(wuss, "Width");
  items[PN_ST_HEIGHT_LABEL].axis_size = task_text_width(wuss, "Height");
  items[PN_ST_BG_LABEL].axis_size     = task_text_width(wuss, "Background");
  for (i = 0; i < PN_BG__COUNT; i++)
    items[PN_ST_BG + i].axis_size = wuss_STD_OPTION_SIZE + wuss_STD_GAP +
                                    task_text_width(wuss, paintnew_bg_names[i]);
  items[PN_ST_CANCEL].axis_size =
    task_button_width(wuss, "Cancel", wuss_STD_SECONDARY_BUTTON_BORDER);
  items[PN_ST_CREATE].axis_size =
    task_button_width(wuss, "Create", wuss_STD_PRIMARY_BUTTON_BORDER);

  rc = stack_smallest(items, NELEMS(items), size);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, size->w, size->h);

  return stack_solve(items, NELEMS(items), &root, boxes);
}

static result_t paintnew_build_icons(paintnew_t *pn,
                                     const box_t boxes[PN_ST__LIMIT])
{
  result_t               rc;
  wuss_icon_spec_t       specs[PN_ICON__COUNT];
  wuss_dialogue_action_t actions[2];
  char                   width_text[PN_DIGITS + 2];
  char                   height_text[PN_DIGITS + 2];
  int                    i;

  wuss_icon_spec_label(&specs[PN_ICON_WIDTH_LABEL], boxes[PN_ST_WIDTH_LABEL],
                       "Width", wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  snprintf(width_text, sizeof(width_text), "%d", PN_DEFAULT_W);
  wuss_icon_spec_writable(&specs[PN_ICON_WIDTH], boxes[PN_ST_WIDTH],
                          width_text, sizeof(width_text),
                          wuss_ICON_FLAGS_JUSTIFY_CENTRE);

  wuss_icon_spec_label(&specs[PN_ICON_HEIGHT_LABEL], boxes[PN_ST_HEIGHT_LABEL],
                       "Height", wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  snprintf(height_text, sizeof(height_text), "%d", PN_DEFAULT_H);
  wuss_icon_spec_writable(&specs[PN_ICON_HEIGHT], boxes[PN_ST_HEIGHT],
                          height_text, sizeof(height_text),
                          wuss_ICON_FLAGS_JUSTIFY_CENTRE);

  wuss_icon_spec_label(&specs[PN_ICON_BG_LABEL], boxes[PN_ST_BG_LABEL],
                       "Background", wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  for (i = 0; i < PN_BG__COUNT; i++)
    wuss_icon_spec_radio(&specs[PN_ICON_BG + i], boxes[PN_ST_BG + i],
                         paintnew_bg_names[i], 1);

  wuss_icon_spec_action(&specs[PN_ICON_CANCEL], boxes[PN_ST_CANCEL],
                        "Cancel", 0);
  wuss_icon_spec_action(&specs[PN_ICON_CREATE], boxes[PN_ST_CREATE],
                        "Create", 1);

  rc = wuss_icon_create_array(wuss_dialogue_window(pn->dialogue), specs,
                              PN_ICON__COUNT, pn->icons);
  if (rc != result_OK)
    return rc;

  wuss_icon_set_selected(wuss_dialogue_window(pn->dialogue),
                         pn->icons[PN_ICON_BG + PN_BG_WHITE], 1);

  actions[0].icon = pn->icons[PN_ICON_CANCEL];
  actions[0].fn   = paintnew_action_cancel;
  actions[1].icon = pn->icons[PN_ICON_CREATE];
  actions[1].fn   = paintnew_action_create;

  return wuss_dialogue_set_actions(pn->dialogue, actions, 2);
}

/* the dialogue's own task: not autoclose, so it survives its window being
 * dismissed and shown again. Its QUIT, while the window is still alive,
 * frees everything */
static result_t paintnew_handle(wuss_window_t      *window,
                                const wuss_event_t *event,
                                void               *task_data)
{
  paintnew_t *pn;
  result_t    rc;

  (void) window;

  pn = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    rc = result_OK;
    (void) wuss_dialogue_handle_icon(pn->dialogue, event, &rc);
    return rc;

  case wuss_EVENT_KEY:
    rc = result_WUSS_KEY_UNCLAIMED;
    (void) wuss_dialogue_handle_key(pn->dialogue, event, &rc);
    return rc;

  case wuss_EVENT_MENU_CLOSED:
    pn->menu_handle = NULL; /* wuss closed the chain under us */
    return result_OK;

  case wuss_EVENT_QUIT:
    wuss_dialogue_destroy(pn->dialogue);
    *pn->slot = NULL;
    free(pn);
    return result_OK;

  default:
    return result_OK;
  }
}

result_t paintnew_create(paintnew_t          **slot,
                         wuss_t               *wuss,
                         paintnew_accept_fn_t *accept,
                         void                 *opaque)
{
  result_t         rc;
  paintnew_t      *pn;
  wuss_task_desc_t desc;
  size2d_t         size;
  box_t            boxes[PN_ST__LIMIT];

  pn = calloc(1, sizeof(*pn));
  if (pn == NULL)
    return result_OOM;

  pn->slot   = slot;
  pn->wuss   = wuss;
  pn->accept = accept;
  pn->opaque = opaque;

  desc.handle    = paintnew_handle;
  desc.task_data = pn;
  desc.name      = "paint-new";
  rc = wuss_task_create(wuss, &desc, &pn->task);
  if (rc != result_OK)
  {
    free(pn); /* nothing registered yet */
    return rc;
  }

  rc = paintnew_layout(wuss, &size, boxes);
  if (rc == result_OK)
    rc = wuss_dialogue_create(&pn->dialogue, pn->task, size, "New", NULL, pn);
  if (rc == result_OK)
    rc = paintnew_build_icons(pn, boxes);
  if (rc != result_OK)
  {
    wuss_task_destroy(pn->task); /* its QUIT frees pn */
    return rc;
  }

  *slot = pn;

  return result_OK;
}

result_t paintnew_open(paintnew_t *pn)
{
  result_t       rc;
  wuss_window_t *win;
  box_t          content;
  point_t        at;

  win = wuss_dialogue_window(pn->dialogue);
  wuss_window_get_content_bounds(win, &content);

  /* centred on the pointer, standing the same distance off the screen's
   * bottom edge as an icon bar menu, as it is opened from the icon bar */
  at.x = wuss_get_pointer(pn->wuss).x - (content.x1 - content.x0) / 2;
  at.y = wuss_get_screen_size(pn->wuss).h - PN_BAR_GAP -
         (content.y1 - content.y0);

  rc = wuss_menu_open_window(pn->task, win, at, &pn->menu_handle);
  if (rc != result_OK)
    return rc;

  (void) wuss_icon_set_caret(win, pn->icons[PN_ICON_WIDTH], -1);

  return result_OK;
}

#endif /* WUSS_APP */
