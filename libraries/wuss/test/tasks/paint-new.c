/* wuss/test/tasks/paint-new.c -- paint's New dialogue */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "wuss/component/dialogue.h"
#include "wuss/icon.h"
#include "wuss/icon-spec.h"
#include "wuss/menu.h"
#include "wuss/task.h"
#include "wuss/window.h"

#include "paint-new.h"

/* ----------------------------------------------------------------------- */

#define PN_W        196
#define PN_H        144
#define PN_MARGIN   8
#define PN_ROW      28   /* pitch of the width and height rows */
#define PN_FIELD_H  22
#define PN_LABEL_W  56
#define PN_FIELD_X  (PN_MARGIN + PN_LABEL_W + 4)
#define PN_FIELD_W  60
#define PN_DIGITS   4    /* PAINTNEW_SIZE_MAX is four digits */
#define PN_BG_Y     (PN_MARGIN + 2 * PN_ROW)
#define PN_RADIO_W  56
#define PN_BUTTON_W 64
#define PN_BUTTON_Y (PN_BG_Y + 38)

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

static result_t paintnew_build_icons(paintnew_t *pn)
{
  result_t               rc;
  wuss_icon_spec_t       specs[PN_ICON__COUNT];
  wuss_dialogue_action_t actions[2];
  char                   width_text[PN_DIGITS + 2];
  char                   height_text[PN_DIGITS + 2];
  int                    i, y;

  /* width and height: a label and a writable on each row */
  y = PN_MARGIN;
  wuss_icon_spec_label(&specs[PN_ICON_WIDTH_LABEL],
                       (box_t) BOX_POS_SIZE(PN_MARGIN, y, PN_LABEL_W,
                                            PN_FIELD_H),
                       "Width", 0);
  snprintf(width_text, sizeof(width_text), "%d", PN_DEFAULT_W);
  wuss_icon_spec_writable(&specs[PN_ICON_WIDTH],
                          (box_t) BOX_POS_SIZE(PN_FIELD_X, y, PN_FIELD_W,
                                               PN_FIELD_H),
                          width_text, sizeof(width_text),
                          wuss_ICON_FLAGS_JUSTIFY_CENTRE);

  y += PN_ROW;
  wuss_icon_spec_label(&specs[PN_ICON_HEIGHT_LABEL],
                       (box_t) BOX_POS_SIZE(PN_MARGIN, y, PN_LABEL_W,
                                            PN_FIELD_H),
                       "Height", 0);
  snprintf(height_text, sizeof(height_text), "%d", PN_DEFAULT_H);
  wuss_icon_spec_writable(&specs[PN_ICON_HEIGHT],
                          (box_t) BOX_POS_SIZE(PN_FIELD_X, y, PN_FIELD_W,
                                               PN_FIELD_H),
                          height_text, sizeof(height_text),
                          wuss_ICON_FLAGS_JUSTIFY_CENTRE);

  /* background: a caption over a row of radios */
  wuss_icon_spec_label(&specs[PN_ICON_BG_LABEL],
                       (box_t) BOX_POS_SIZE(PN_MARGIN, PN_BG_Y,
                                            PN_W - 2 * PN_MARGIN, 16),
                       "Background", 0);
  for (i = 0; i < PN_BG__COUNT; i++)
    wuss_icon_spec_radio(&specs[PN_ICON_BG + i],
                         (box_t) BOX_POS_SIZE(PN_MARGIN + i * PN_RADIO_W,
                                              PN_BG_Y + 18, PN_RADIO_W, 16),
                         paintnew_bg_names[i], 1);

  /* Cancel centred against the taller, default Create */
  wuss_icon_spec_action(&specs[PN_ICON_CANCEL],
                        (box_t) BOX_POS_SIZE(PN_MARGIN, PN_BUTTON_Y + 4,
                                             PN_BUTTON_W, 24),
                        "Cancel", 0);
  wuss_icon_spec_action(&specs[PN_ICON_CREATE],
                        (box_t) BOX_POS_SIZE(PN_W - PN_MARGIN - PN_BUTTON_W,
                                             PN_BUTTON_Y, PN_BUTTON_W, 32),
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

  rc = wuss_dialogue_create(&pn->dialogue, pn->task, SIZE2D(PN_W, PN_H),
                            "New", NULL, pn);
  if (rc == result_OK)
    rc = paintnew_build_icons(pn);
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
