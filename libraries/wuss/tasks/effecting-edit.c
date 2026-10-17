/* wuss/tasks/effecting-edit.c -- effecting task's editing dialogues */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/tonemap.h"
#include "wuss/component/dialogue.h"
#include "wuss/icon.h"
#include "wuss/icon-spec.h"

#include "effecting-edit.h"
#include "common.h"

#define EDIT_PLOT_SIZE 128
#define EDIT_SLIDER_W  128
#define EDIT_NCHANNELS 5
#define EDIT_NTONE     5 /* sliders in the tone dialogue */

/* tone dialogue icons: radios, preview, then label/slider/value per row,
 * then the options and Reset */
#define TONE_ICON_RADIO(i)   (i)
#define TONE_ICON_PREVIEW    EDIT_NCHANNELS
#define TONE_ICON_LABEL(i)   (TONE_ICON_PREVIEW + 1 + (i))
#define TONE_ICON_SLIDER(i)  (TONE_ICON_LABEL(0) + EDIT_NTONE + (i))
#define TONE_ICON_VALUE(i)   (TONE_ICON_SLIDER(0) + EDIT_NTONE + (i))
#define TONE_ICON_INVERT     (TONE_ICON_VALUE(0) + EDIT_NTONE)
#define TONE_ICON_REFLECT    (TONE_ICON_INVERT + 1)
#define TONE_ICON_RESET      (TONE_ICON_REFLECT + 1)
#define TONE_NICONS          (TONE_ICON_RESET + 1)

typedef struct effecting_dlg
{
  effecting_edit_t      *edit;
  effecting_edit_which_t which;
  wuss_dialogue_t       *dialogue;
}
effecting_dlg_t;

struct effecting_edit
{
  effecting_task_t   *ec;
  effecting_dlg_t     dlg[EFFECTING_EDIT__LIMIT];

  wuss_icon_t        *blur_radio[2];
  wuss_slider_row_t   blur_row;

  wuss_slider_row_t   level_row;

  wuss_icon_t        *tone_radio[EDIT_NCHANNELS];
  wuss_slider_row_t   tone_row[EDIT_NTONE];
  wuss_icon_t        *tone_invert;
  wuss_icon_t        *tone_reflect;
  box_t               tone_preview;
  int                 tone_channel; /* index into g_channels */
  bitmap_t            plot;
  unsigned char       plot_pixels[EDIT_PLOT_SIZE * EDIT_PLOT_SIZE * 4];
};

static const struct
{
  const char        *label;
  tonemap_channels_t channels;
}
g_channels[EDIT_NCHANNELS] =
{
  { "RGB",   tonemap_CHANNEL_RGB },
  { "Red",   tonemap_CHANNEL_RED },
  { "Green", tonemap_CHANNEL_GREEN },
  { "Blue",  tonemap_CHANNEL_BLUE },
  { "Alpha", tonemap_CHANNEL_ALPHA }
};

static const struct
{
  const char *label;
  int         min, max;
}
g_tone_rows[EDIT_NTONE] =
{
  { "Gamma",      10, 290 },
  { "Brightness",  0, 200 },
  { "Contrast",    0, 200 },
  { "Bias",        0, 100 },
  { "Gain",        0, 100 }
};

/* ----------------------------------------------------------------------- */

static int *edit_tone_field(tonemap_spec_t *spec, int row)
{
  switch (row)
  {
  case 0:  return &spec->gamma;
  case 1:  return &spec->brightness;
  case 2:  return &spec->contrast;
  case 3:  return &spec->bias;
  default: return &spec->gain;
  }
}

/* the channel whose values the dialogue shows: RGB reads red's */
static tonemap_channels_t edit_tone_read_channel(const effecting_edit_t *edit)
{
  tonemap_channels_t channels;

  channels = g_channels[edit->tone_channel].channels;
  if (channels == tonemap_CHANNEL_RGB)
    channels = tonemap_CHANNEL_RED;

  return channels;
}

/* fills in one slider row's three icon specs; the label is right-justified
 * in its own box */
static void edit_row_specs(wuss_icon_spec_t *label,
                           wuss_icon_spec_t *slider,
                           wuss_icon_spec_t *value,
                           int               x,
                           int               y,
                           int               label_w,
                           int               value_w,
                           const char       *text,
                           int               min,
                           int               max,
                           int               current,
                           const char       *fmt,
                           char             *buf)
{
  box_t label_box, slider_box, value_box;

  label_box  = (box_t) BOX_POS_SIZE(x, y, label_w, wuss_STD_SLIDER_HEIGHT);
  slider_box = (box_t) BOX_POS_SIZE(x + label_w + wuss_STD_GAP, y,
                                    EDIT_SLIDER_W, wuss_STD_SLIDER_HEIGHT);
  value_box  = (box_t) BOX_POS_SIZE(x + label_w + EDIT_SLIDER_W +
                                    2 * wuss_STD_GAP, y, value_w,
                                    wuss_STD_SLIDER_HEIGHT);

  wuss_icon_spec_label(label, label_box, text, wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_slider_row(slider, value, slider_box, value_box,
                            wuss_SLIDER_HORIZONTAL, min, max, current, fmt, 0,
                            buf, WUSS_SLIDER_ROW_BUF);
}

/* widest of two values' text, for a row's value box */
static int edit_value_width(const wuss_t *wuss,
                            const char   *fmt,
                            int           min,
                            int           max)
{
  char buf[WUSS_SLIDER_ROW_BUF];
  int  w;

  snprintf(buf, sizeof(buf), fmt, min);
  w = task_text_width(wuss, buf);
  snprintf(buf, sizeof(buf), fmt, max);

  return MAX(w, task_text_width(wuss, buf));
}

/* ----- blur ----- */

static result_t edit_blur_fillout(void *opaque)
{
  effecting_dlg_t     *dlg;
  effecting_edit_t    *edit;
  effecting_element_t *el;
  wuss_window_t       *window;
  result_t             rc;

  dlg    = opaque;
  edit   = dlg->edit;
  window = wuss_dialogue_window(dlg->dialogue);
  el     = effecting_selected(edit->ec, NULL);
  if (el == NULL || el->type != EFFECTING_BLUR)
    return result_OK;

  wuss_icon_set_selected(window, edit->blur_radio[0],
                         el->u.blur.method == effects_blur_BOX);
  wuss_icon_set_selected(window, edit->blur_radio[1],
                         el->u.blur.method == effects_blur_GAUSSIAN);
  rc = wuss_slider_row_set(window, &edit->blur_row, el->u.blur.amount);

  return rc;
}

static result_t edit_blur_create(effecting_edit_t *edit)
{
  result_t         rc;
  effecting_dlg_t *dlg;
  wuss_t          *wuss;
  wuss_icon_spec_t specs[5];
  wuss_icon_t     *made[5];
  char             buf[WUSS_SLIDER_ROW_BUF];
  int              label_w, value_w, radio_w, width, height;
  int              x, y;
  box_t            box;

  dlg  = &edit->dlg[EFFECTING_EDIT_BLUR];
  wuss = edit->ec->wuss;

  label_w = task_text_width(wuss, "Radius");
  value_w = edit_value_width(wuss, "%d", effects_blur_MIN_AMOUNT,
                             effects_blur_MAX_AMOUNT);
  radio_w = wuss_STD_GLYPH_SIZE + wuss_STD_GAP +
            MAX(task_text_width(wuss, "Box"), task_text_width(wuss, "Gaussian"));
  width   = 2 * wuss_STD_INSET + label_w + EDIT_SLIDER_W + value_w +
            2 * wuss_STD_GAP;
  width   = MAX(width, 2 * wuss_STD_INSET + 2 * radio_w + wuss_STD_GAP);
  height  = 2 * wuss_STD_INSET + wuss_STD_GLYPH_SIZE + wuss_STD_GAP +
            wuss_STD_SLIDER_HEIGHT;

  rc = wuss_dialogue_create(&dlg->dialogue, edit->ec->delegate,
                            SIZE2D(width, height), "Blur", edit_blur_fillout,
                            dlg);
  if (rc != result_OK)
    return rc;

  wuss_dialogue_set_focusable(dlg->dialogue, 0);

  x   = wuss_STD_INSET;
  y   = wuss_STD_INSET;
  box = (box_t) BOX_POS_SIZE(x, y, radio_w, wuss_STD_GLYPH_SIZE);
  wuss_icon_spec_radio(&specs[0], box, "Box", 1);
  box = (box_t) BOX_POS_SIZE(x + radio_w + wuss_STD_GAP, y, radio_w,
                             wuss_STD_GLYPH_SIZE);
  wuss_icon_spec_radio(&specs[1], box, "Gaussian", 1);

  y += wuss_STD_GLYPH_SIZE + wuss_STD_GAP;
  edit_row_specs(&specs[2], &specs[3], &specs[4], x, y, label_w, value_w,
                 "Radius", effects_blur_MIN_AMOUNT, effects_blur_MAX_AMOUNT,
                 effects_blur_MIN_AMOUNT, NULL, buf);

  rc = wuss_icon_create_array(wuss_dialogue_window(dlg->dialogue), specs,
                              NELEMS(specs), made);
  if (rc != result_OK)
  {
    wuss_dialogue_destroy(dlg->dialogue);
    dlg->dialogue = NULL;
    return rc;
  }

  edit->blur_radio[0] = made[0];
  edit->blur_radio[1] = made[1];
  wuss_slider_row_bind(&edit->blur_row, made[3], made[4], NULL,
                       effects_blur_MIN_AMOUNT, effects_blur_MAX_AMOUNT, 0);

  return result_OK;
}

static result_t edit_blur_icon(effecting_edit_t   *edit,
                               const wuss_event_t *event)
{
  effecting_element_t *el;
  wuss_window_t       *window;
  int                  value;
  int                  i;

  window = wuss_dialogue_window(edit->dlg[EFFECTING_EDIT_BLUR].dialogue);
  el     = effecting_selected(edit->ec, NULL);
  if (el == NULL || el->type != EFFECTING_BLUR)
    return result_OK;

  if (wuss_slider_row_event(window, &edit->blur_row, event, &value))
  {
    el->u.blur.amount = value;
    effecting_update(edit->ec, 1);
    return result_OK;
  }

  if (event->data.icon.action != wuss_MOUSE_UP)
    return result_OK;

  for (i = 0; i < 2; i++)
  {
    if (event->data.icon.icon != edit->blur_radio[i])
      continue;

    el->u.blur.method = i == 0 ? effects_blur_BOX : effects_blur_GAUSSIAN;
    effecting_update(edit->ec, 1);
  }

  return result_OK;
}

/* ----- level ----- */

static result_t edit_level_fillout(void *opaque)
{
  effecting_dlg_t *dlg;

  dlg = opaque;

  return wuss_slider_row_set(wuss_dialogue_window(dlg->dialogue),
                             &dlg->edit->level_row, dlg->edit->ec->level);
}

static result_t edit_level_create(effecting_edit_t *edit)
{
  result_t         rc;
  effecting_dlg_t *dlg;
  wuss_t          *wuss;
  wuss_icon_spec_t specs[3];
  wuss_icon_t     *made[3];
  char             buf[WUSS_SLIDER_ROW_BUF];
  int              label_w, value_w, width, height;

  dlg  = &edit->dlg[EFFECTING_EDIT_LEVEL];
  wuss = edit->ec->wuss;

  label_w = task_text_width(wuss, "Level");
  value_w = edit_value_width(wuss, "%d%%", EFFECTING_LEVEL_MIN,
                             EFFECTING_LEVEL_MAX);
  width   = 2 * wuss_STD_INSET + label_w + EDIT_SLIDER_W + value_w +
            2 * wuss_STD_GAP;
  height  = 2 * wuss_STD_INSET + wuss_STD_SLIDER_HEIGHT;

  rc = wuss_dialogue_create(&dlg->dialogue, edit->ec->delegate,
                            SIZE2D(width, height), "Level",
                            edit_level_fillout, dlg);
  if (rc != result_OK)
    return rc;

  wuss_dialogue_set_focusable(dlg->dialogue, 0);

  edit_row_specs(&specs[0], &specs[1], &specs[2], wuss_STD_INSET,
                 wuss_STD_INSET, label_w, value_w, "Level",
                 EFFECTING_LEVEL_MIN, EFFECTING_LEVEL_MAX, edit->ec->level,
                 "%d%%", buf);

  rc = wuss_icon_create_array(wuss_dialogue_window(dlg->dialogue), specs,
                              NELEMS(specs), made);
  if (rc != result_OK)
  {
    wuss_dialogue_destroy(dlg->dialogue);
    dlg->dialogue = NULL;
    return rc;
  }

  wuss_slider_row_bind(&edit->level_row, made[1], made[2], "%d%%",
                       EFFECTING_LEVEL_MIN, EFFECTING_LEVEL_MAX, 0);

  return result_OK;
}

static result_t edit_level_icon(effecting_edit_t   *edit,
                                const wuss_event_t *event)
{
  int value;

  if (!wuss_slider_row_event(
        wuss_dialogue_window(edit->dlg[EFFECTING_EDIT_LEVEL].dialogue),
        &edit->level_row, event, &value))
    return result_OK;

  edit->ec->level = value;
  effecting_update(edit->ec, 0);

  return result_OK;
}

/* ----- tone ----- */

/* redraws the curve preview from the selected element */
static void edit_tone_plot(effecting_edit_t *edit, tonemap_t *map)
{
  screen_t scr;
  box_t    area;

  area = (box_t) BOX_POS_SIZE(0, 0, EDIT_PLOT_SIZE, EDIT_PLOT_SIZE);

  screen_for_bitmap(&scr, &edit->plot);
  tonemap_plot(map, g_channels[edit->tone_channel].channels, &scr, &area, 1);

  wuss_window_invalidate(
    wuss_dialogue_window(edit->dlg[EFFECTING_EDIT_TONE].dialogue),
    &edit->tone_preview);
}

static result_t edit_tone_fillout(void *opaque)
{
  effecting_dlg_t     *dlg;
  effecting_edit_t    *edit;
  effecting_element_t *el;
  wuss_window_t       *window;
  tonemap_spec_t       spec;
  result_t             rc;
  int                  i;

  dlg    = opaque;
  edit   = dlg->edit;
  window = wuss_dialogue_window(dlg->dialogue);
  el     = effecting_selected(edit->ec, NULL);
  if (el == NULL || el->type != EFFECTING_TONE)
    return result_OK;

  tonemap_get_values(el->u.tone, edit_tone_read_channel(edit), &spec);

  rc = result_OK;
  for (i = 0; i < EDIT_NTONE; i++)
    rc = wuss_slider_row_set(window, &edit->tone_row[i],
                             *edit_tone_field(&spec, i));

  wuss_icon_set_selected(window, edit->tone_invert,
                         (spec.flags & tonemap_FLAG_INVERT) != 0);
  wuss_icon_set_selected(window, edit->tone_reflect,
                         (spec.flags & tonemap_FLAG_REFLECT) != 0);
  for (i = 0; i < EDIT_NCHANNELS; i++)
    wuss_icon_set_selected(window, edit->tone_radio[i],
                           i == edit->tone_channel);

  edit_tone_plot(edit, el->u.tone);

  return rc;
}

static result_t edit_tone_reset(void *opaque, wuss_button_t button)
{
  effecting_dlg_t     *dlg;
  effecting_element_t *el;

  NOT_USED(button);

  dlg = opaque;
  el  = effecting_selected(dlg->edit->ec, NULL);
  if (el == NULL || el->type != EFFECTING_TONE)
    return result_OK;

  tonemap_reset(el->u.tone);
  effecting_update(dlg->edit->ec, 1);

  return edit_tone_fillout(dlg);
}

static result_t edit_tone_create(effecting_edit_t *edit)
{
  result_t               rc;
  effecting_dlg_t       *dlg;
  wuss_t                *wuss;
  wuss_icon_spec_t       specs[TONE_NICONS];
  wuss_icon_t           *made[TONE_NICONS];
  char                   bufs[EDIT_NTONE][WUSS_SLIDER_ROW_BUF];
  wuss_dialogue_action_t action;
  int                    label_w, value_w, radios_w, opts_w, width, height;
  int                    x, y, i;
  int                    radio_w[EDIT_NCHANNELS];
  box_t                  box;

  dlg  = &edit->dlg[EFFECTING_EDIT_TONE];
  wuss = edit->ec->wuss;

  label_w = 0;
  for (i = 0; i < EDIT_NTONE; i++)
    label_w = MAX(label_w, task_text_width(wuss, g_tone_rows[i].label));
  value_w = edit_value_width(wuss, "%d", 0, 290);

  radios_w = 0;
  for (i = 0; i < EDIT_NCHANNELS; i++)
  {
    radio_w[i] = wuss_STD_GLYPH_SIZE + wuss_STD_GAP +
                 task_text_width(wuss, g_channels[i].label);
    radios_w  += radio_w[i] + wuss_STD_GAP;
  }
  radios_w -= wuss_STD_GAP;

  opts_w = 2 * wuss_STD_GLYPH_SIZE + 3 * wuss_STD_GAP +
           task_text_width(wuss, "Invert") + task_text_width(wuss, "Reflect");

  width  = 2 * wuss_STD_INSET + label_w + EDIT_SLIDER_W + value_w +
           2 * wuss_STD_GAP;
  width  = MAX(width, 2 * wuss_STD_INSET + radios_w);
  width  = MAX(width, 2 * wuss_STD_INSET + opts_w + wuss_STD_GAP +
               task_button_width(wuss, "Reset",
                                 wuss_STD_SECONDARY_BUTTON_BORDER));
  height = 2 * wuss_STD_INSET + wuss_STD_GLYPH_SIZE + wuss_STD_GAP +
           EDIT_PLOT_SIZE + wuss_STD_GAP +
           EDIT_NTONE * (wuss_STD_SLIDER_HEIGHT + wuss_STD_GAP) +
           wuss_STD_SECONDARY_BUTTON_HEIGHT;

  rc = bitmap_init(&edit->plot, SIZE2D(EDIT_PLOT_SIZE, EDIT_PLOT_SIZE),
                   pixelfmt_rgbx8888, EDIT_PLOT_SIZE * 4, NULL,
                   edit->plot_pixels);
  if (rc != result_OK)
    return rc;

  rc = wuss_dialogue_create(&dlg->dialogue, edit->ec->delegate,
                            SIZE2D(width, height), "Tone",
                            edit_tone_fillout, dlg);
  if (rc != result_OK)
    return rc;

  wuss_dialogue_set_focusable(dlg->dialogue, 0);

  x = wuss_STD_INSET;
  y = wuss_STD_INSET;
  for (i = 0; i < EDIT_NCHANNELS; i++)
  {
    box = (box_t) BOX_POS_SIZE(x, y, radio_w[i], wuss_STD_GLYPH_SIZE);
    wuss_icon_spec_radio(&specs[TONE_ICON_RADIO(i)], box,
                         g_channels[i].label, 1);
    x += radio_w[i] + wuss_STD_GAP;
  }

  y += wuss_STD_GLYPH_SIZE + wuss_STD_GAP;
  x  = (width - EDIT_PLOT_SIZE) / 2;
  edit->tone_preview = (box_t) BOX_POS_SIZE(x, y, EDIT_PLOT_SIZE,
                                            EDIT_PLOT_SIZE);
  memset(&specs[TONE_ICON_PREVIEW], 0, sizeof(specs[TONE_ICON_PREVIEW]));
  specs[TONE_ICON_PREVIEW].bbox           = edit->tone_preview;
  specs[TONE_ICON_PREVIEW].type           = wuss_ICON_TYPE_BITMAP;
  specs[TONE_ICON_PREVIEW].bg             = wuss_NO_BACKGROUND;
  specs[TONE_ICON_PREVIEW].u.bitmap.image = &edit->plot;

  y += EDIT_PLOT_SIZE + wuss_STD_GAP;
  x  = wuss_STD_INSET;
  for (i = 0; i < EDIT_NTONE; i++)
  {
    edit_row_specs(&specs[TONE_ICON_LABEL(i)], &specs[TONE_ICON_SLIDER(i)],
                   &specs[TONE_ICON_VALUE(i)], x, y, label_w, value_w,
                   g_tone_rows[i].label, g_tone_rows[i].min,
                   g_tone_rows[i].max, g_tone_rows[i].min, NULL, bufs[i]);
    y += wuss_STD_SLIDER_HEIGHT + wuss_STD_GAP;
  }

  box = (box_t) BOX_POS_SIZE(x, y,
                             wuss_STD_GLYPH_SIZE + wuss_STD_GAP +
                             task_text_width(wuss, "Invert"),
                             wuss_STD_GLYPH_SIZE);
  wuss_icon_spec_option(&specs[TONE_ICON_INVERT], box, "Invert");
  x  += box.x1 - box.x0 + wuss_STD_GAP;
  box = (box_t) BOX_POS_SIZE(x, y,
                             wuss_STD_GLYPH_SIZE + wuss_STD_GAP +
                             task_text_width(wuss, "Reflect"),
                             wuss_STD_GLYPH_SIZE);
  wuss_icon_spec_option(&specs[TONE_ICON_REFLECT], box, "Reflect");

  box = (box_t) BOX_POS_SIZE(width - wuss_STD_INSET -
                             task_button_width(wuss, "Reset",
                               wuss_STD_SECONDARY_BUTTON_BORDER),
                             height - wuss_STD_INSET -
                             wuss_STD_SECONDARY_BUTTON_HEIGHT,
                             task_button_width(wuss, "Reset",
                               wuss_STD_SECONDARY_BUTTON_BORDER),
                             wuss_STD_SECONDARY_BUTTON_HEIGHT);
  wuss_icon_spec_action(&specs[TONE_ICON_RESET], box, "Reset", 0);

  rc = wuss_icon_create_array(wuss_dialogue_window(dlg->dialogue), specs,
                              TONE_NICONS, made);
  if (rc != result_OK)
    goto failure;

  for (i = 0; i < EDIT_NCHANNELS; i++)
    edit->tone_radio[i] = made[TONE_ICON_RADIO(i)];
  for (i = 0; i < EDIT_NTONE; i++)
    wuss_slider_row_bind(&edit->tone_row[i], made[TONE_ICON_SLIDER(i)],
                         made[TONE_ICON_VALUE(i)], NULL, g_tone_rows[i].min,
                         g_tone_rows[i].max, 0);
  edit->tone_invert  = made[TONE_ICON_INVERT];
  edit->tone_reflect = made[TONE_ICON_REFLECT];

  action.icon = made[TONE_ICON_RESET];
  action.fn   = edit_tone_reset;
  rc = wuss_dialogue_set_actions(dlg->dialogue, &action, 1);
  if (rc != result_OK)
    goto failure;

  return result_OK;

failure:
  wuss_dialogue_destroy(dlg->dialogue);
  dlg->dialogue = NULL;

  return rc;
}

static result_t edit_tone_icon(effecting_edit_t   *edit,
                               const wuss_event_t *event)
{
  result_t             rc;
  effecting_element_t *el;
  wuss_window_t       *window;
  tonemap_spec_t       spec;
  int                  value;
  int                  i;

  window = wuss_dialogue_window(edit->dlg[EFFECTING_EDIT_TONE].dialogue);
  el     = effecting_selected(edit->ec, NULL);
  if (el == NULL || el->type != EFFECTING_TONE)
    return result_OK;

  for (i = 0; i < EDIT_NTONE; i++)
  {
    if (!wuss_slider_row_event(window, &edit->tone_row[i], event, &value))
      continue;

    tonemap_get_values(el->u.tone, edit_tone_read_channel(edit), &spec);
    *edit_tone_field(&spec, i) = value;
    tonemap_set(el->u.tone, g_channels[edit->tone_channel].channels, &spec);
    edit_tone_plot(edit, el->u.tone);
    effecting_update(edit->ec, 1);

    return result_OK;
  }

  if (event->data.icon.action == wuss_MOUSE_UP)
  {
    for (i = 0; i < EDIT_NCHANNELS; i++)
    {
      if (event->data.icon.icon != edit->tone_radio[i])
        continue;

      edit->tone_channel = i;

      return edit_tone_fillout(&edit->dlg[EFFECTING_EDIT_TONE]);
    }

    if (event->data.icon.icon == edit->tone_invert ||
        event->data.icon.icon == edit->tone_reflect)
    {
      tonemap_get_values(el->u.tone, edit_tone_read_channel(edit), &spec);
      spec.flags = 0;
      if (wuss_icon_get_selected(edit->tone_invert))
        spec.flags |= tonemap_FLAG_INVERT;
      if (wuss_icon_get_selected(edit->tone_reflect))
        spec.flags |= tonemap_FLAG_REFLECT;
      tonemap_set(el->u.tone, g_channels[edit->tone_channel].channels, &spec);
      edit_tone_plot(edit, el->u.tone);
      effecting_update(edit->ec, 1);

      return result_OK;
    }
  }

  if (wuss_dialogue_handle_icon(edit->dlg[EFFECTING_EDIT_TONE].dialogue,
                                event, &rc))
    return rc;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t effecting_edit_create(effecting_task_t *ec, effecting_edit_t **out)
{
  result_t          rc;
  effecting_edit_t *edit;
  int               i;

  edit = calloc(1, sizeof(*edit));
  if (edit == NULL)
    return result_OOM;

  edit->ec = ec;
  for (i = 0; i < EFFECTING_EDIT__LIMIT; i++)
  {
    edit->dlg[i].edit  = edit;
    edit->dlg[i].which = i;
  }

  rc = edit_blur_create(edit);
  if (rc == result_OK)
    rc = edit_tone_create(edit);
  if (rc == result_OK)
    rc = edit_level_create(edit);
  if (rc != result_OK)
  {
    effecting_edit_destroy(edit);
    return rc;
  }

  *out = edit;

  return result_OK;
}

void effecting_edit_closed(effecting_edit_t *edit, wuss_window_t *window)
{
  int i;

  if (edit == NULL)
    return;

  for (i = 0; i < EFFECTING_EDIT__LIMIT; i++)
    wuss_dialogue_handle_close(edit->dlg[i].dialogue, window);
}

void effecting_edit_destroy(effecting_edit_t *edit)
{
  int i;

  if (edit == NULL)
    return;

  for (i = 0; i < EFFECTING_EDIT__LIMIT; i++)
    wuss_dialogue_destroy(edit->dlg[i].dialogue);
  free(edit);
}

wuss_window_t *effecting_edit_window(const effecting_edit_t *edit,
                                     effecting_edit_which_t  which)
{
  return wuss_dialogue_window(edit->dlg[which].dialogue);
}

int effecting_edit_find(const effecting_edit_t *edit,
                        const wuss_window_t    *window)
{
  int i;

  for (i = 0; i < EFFECTING_EDIT__LIMIT; i++)
    if (edit->dlg[i].dialogue != NULL &&
        wuss_dialogue_window(edit->dlg[i].dialogue) == window)
      return i;

  return -1;
}

result_t effecting_edit_icon(effecting_edit_t      *edit,
                             effecting_edit_which_t which,
                             const wuss_event_t    *event)
{
  switch (which)
  {
  case EFFECTING_EDIT_BLUR:
    return edit_blur_icon(edit, event);

  case EFFECTING_EDIT_TONE:
    return edit_tone_icon(edit, event);

  default:
    return edit_level_icon(edit, event);
  }
}

result_t effecting_edit_pre_show(effecting_edit_t      *edit,
                                 effecting_edit_which_t which)
{
  return wuss_dialogue_handle_pre_show(edit->dlg[which].dialogue);
}

#endif /* WUSS_APP */
