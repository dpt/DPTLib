/* wuss/test/tasks/particles.c -- particle explosion task */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "geom/size.h"
#include "geom/stack.h"

#include "common.h"
#include "particles.h"

#define PARTICLES_WIDTH     480
#define PARTICLES_HEIGHT    384

#define PARTICLES_BURST (MAX_PARTICLES / 2) /* particles per click */

/* pointer trail, as Explosion's playground */
#define PARTICLES_TRAIL_RATE    5.0f  /* particles per second */
#define PARTICLES_TRAIL_MAX     10    /* cap per idle tick */
#define PARTICLES_TRAIL_DAMPING 0.25f /* fraction of pointer velocity kept */

/* MENU click pops this menu; the item table and wuss_menu_t live
 * per-instance in particles_task_t, not as a file-scope static, so that each
 * window's Info row can hold its own .window pointer to the shared proginfo
 * singleton, retargeted just before wuss_menu_open */
enum
{
  PARTICLES_MENU_INFO,
  PARTICLES_MENU_ADD,
  PARTICLES_MENU_BACKGROUND,
  PARTICLES_MENU_PAUSE,
  PARTICLES_MENU_GRAVITY,
  PARTICLES_MENU_WALLS,
  PARTICLES_MENU_MARKERS,
  PARTICLES_MENU_CLEAR
};

/* "Add" submenu rows */
enum
{
  PARTICLES_ADD_EMITTER,
  PARTICLES_ADD_ATTRACTOR,
  PARTICLES_ADD_REPELLER
};

/* "Gravity" submenu rows: each scales every style's own gravity */
static const struct
{
  const char *name;
  float       scale;
}
particles_gravities[] =
{
  { "Off",       0.0f },
  { "Low",       0.5f },
  { "Normal",    1.0f },
  { "High",      2.0f },
  { "Reversed", -1.0f }
};

#define PARTICLES_GRAVITY_NORMAL 2

/* "Add emitter" submenu rows; picking one adds an emitter at that intensity.
 * wuss never picks a row that has a submenu, so the intensity rows are
 * what add the emitter rather than the "Add emitter" row itself. */
static const struct
{
  const char *name;
  float       rate; /* particles per second */
}
particles_intensities[] =
{
  { "Low",     5.0f },
  { "Medium", 20.0f },
  { "High",   80.0f }
};

/* Add > Attractor/Repeller's shared Strength dialogue: a 1..100 slider,
 * scaled to a force strength (attractor: create_repeller with strength
 * negated -- same force, opposite sign) and a second slider for the range
 * (max_distance, in pixels) of its sphere of influence. */
#define PARTICLES_STRENGTH_MIN         1
#define PARTICLES_STRENGTH_MAX         100
#define PARTICLES_STRENGTH_SCALE       1000.0f /* slider value -> strength */
#define PARTICLES_STRENGTH_DEFAULT     20
#define PARTICLES_RANGE_MIN            10
#define PARTICLES_RANGE_MAX            200
#define PARTICLES_RANGE_DEFAULT        100 /* pixels of influence */

#define PARTICLES_EMITTER_RING         6 /* radius of an emitter's marker */

/* the Strength dialogue's icons, in creation order (see
 * particles_strength_dialogue_create) */
enum
{
  PARTICLES_STRENGTH_ICON_LABEL = 0,
  PARTICLES_STRENGTH_ICON_SLIDER,
  PARTICLES_STRENGTH_ICON_VALUE,

  PARTICLES_STRENGTH_ICON_RANGE_LABEL,
  PARTICLES_STRENGTH_ICON_RANGE_SLIDER,
  PARTICLES_STRENGTH_ICON_RANGE_VALUE,

  PARTICLES_STRENGTH_ICON_CANCEL,
  PARTICLES_STRENGTH_ICON_APPLY,

  PARTICLES_STRENGTH_NICONS
};

static result_t particles_strength_dialogue_create(particles_task_t *task);
static result_t particles_strength_fillout(void *opaque);
static result_t particles_strength_cancel(void         *opaque,
                                          wuss_button_t button);
static result_t particles_strength_apply_action(void         *opaque,
                                                wuss_button_t button);

/* styles, in the order particles_init_styles sets them up */
enum
{
  PARTICLES_FIREY,
  PARTICLES_SMOKEY,
  PARTICLES_FLECK,
  PARTICLES_PASTEL
};

/* ----------------------------------------------------------------------- */

/* a gradient colour stop; each ramp runs from stop 0.0 to stop 1.0 */
typedef struct
{
  unsigned char r, g, b;
  float         stop;
}
particles_stop_t;

static const particles_stop_t particles_firey[] =
{
  { 255, 255, 255, 0.0f }, /* white */
  { 255, 232,   8, 0.1f }, /* yellow */
  { 255, 206,   0, 0.2f }, /* yellow-orange */
  { 255, 154,   0, 0.5f }, /* orange */
  { 255,  90,   0, 0.6f }, /* red */
  {   0,   0, 127, 1.0f }  /* dark blue */
};

static const particles_stop_t particles_smokey[] =
{
  { 255, 154,   0, 0.0f }, /* orange */
  { 127, 127, 127, 0.4f }, /* mid grey */
  {  31,  31,  31, 0.9f }, /* dark grey */
  {   0,   0,   0, 1.0f }  /* black */
};

static const particles_stop_t particles_fleck[] =
{
  { 255, 255, 255, 0.0f }, /* white */
  { 255, 255,   0, 0.2f }, /* yellow */
  {   0, 255,   0, 0.3f }, /* green */
  {   0, 127,   0, 0.5f }, /* dark green */
  {   0,   0, 127, 0.9f }, /* dark blue */
  {   0,   0,   0, 1.0f }  /* black */
};

static const particles_stop_t particles_pastel[] =
{
  { 251, 243, 185, 0.0f }, /* lemon */
  { 255, 220, 204, 0.3f }, /* peach */
  { 253, 183, 234, 0.7f }, /* pink */
  { 183, 177, 242, 1.0f }  /* mauve */
};

/* fill palette[0..PALETTE_SIZE) by sampling the stops evenly */
static void particles_ramp(const particles_stop_t *stops, colour_t *palette)
{
  int                     i;
  float                   t;
  const particles_stop_t *a, *b;
  float                   u;

  for (i = 0; i < PALETTE_SIZE; i++)
  {
    t = (float) i / (PALETTE_SIZE - 1);

    for (a = stops; t > a[1].stop; a++)
      ;
    b = a + 1;
    u = (t - a->stop) / (b->stop - a->stop);

    palette[i] = colour_rgb((unsigned int) (a->r + (b->r - a->r) * u),
                            (unsigned int) (a->g + (b->g - a->g) * u),
                            (unsigned int) (a->b + (b->b - a->b) * u));
  }
}

/* the same style set as Explosion's own playground */
static void particles_init_styles(particle_style_t *styles)
{
  float frame_ms;

  frame_ms = 1000.0f / PHYSICS_FPS;

  set_default_style(&styles[PARTICLES_FIREY], frame_ms);
  styles[PARTICLES_FIREY].probability    = 90;
  styles[PARTICLES_FIREY].palette_index  = PARTICLES_FIREY;
  styles[PARTICLES_FIREY].emit_angle     = 270.0f;
  styles[PARTICLES_FIREY].emit_range     = 90.0f;

  set_default_style(&styles[PARTICLES_SMOKEY], frame_ms);
  styles[PARTICLES_SMOKEY].probability   = 8;
  styles[PARTICLES_SMOKEY].palette_index = PARTICLES_SMOKEY;
  styles[PARTICLES_SMOKEY].emit_angle    = 270.0f;
  styles[PARTICLES_SMOKEY].emit_range    = 90.0f;
  styles[PARTICLES_SMOKEY].min_life     *= 4;
  styles[PARTICLES_SMOKEY].max_life     *= 4;
  styles[PARTICLES_SMOKEY].vel_scale     = 0.1f;
  styles[PARTICLES_SMOKEY].emit_speed    = 10;
  styles[PARTICLES_SMOKEY].min_size      = 1;
  styles[PARTICLES_SMOKEY].max_size      = 2;
  styles[PARTICLES_SMOKEY].gravity      /= -100.0f;

  set_default_style(&styles[PARTICLES_FLECK], frame_ms);
  styles[PARTICLES_FLECK].probability    = 2;
  styles[PARTICLES_FLECK].palette_index  = PARTICLES_FLECK;
  styles[PARTICLES_FLECK].emit_speed     = 200;

  set_default_style(&styles[PARTICLES_PASTEL], frame_ms);
  styles[PARTICLES_PASTEL].probability   = 0;
  styles[PARTICLES_PASTEL].palette_index = PARTICLES_PASTEL;
  styles[PARTICLES_PASTEL].min_life     /= 2;
  styles[PARTICLES_PASTEL].max_life     /= 2;
  styles[PARTICLES_PASTEL].emit_speed    = 25;
  styles[PARTICLES_PASTEL].gravity      /= 2.0f;
}

/* ----------------------------------------------------------------------- */

/* engine callbacks */

static unsigned int particles_rand(int nbits, void *opaque)
{
  particles_task_t *pt;
  uint32_t          r;

  pt = opaque;

  r = rng_xorshift32(&pt->rng);
  return (nbits >= 32) ? r : r & ((1u << nbits) - 1);
}

static unsigned int particles_time(void *opaque)
{
  particles_task_t *pt;

  pt = opaque;

  return pt->now_ms;
}

/* a filled square centred on (x,y), as Explosion's playground draws it */
static void particles_render(int   x,
                             int   y,
                             int   size,
                             int   palette_index,
                             void *opaque)
{
  particles_task_t *pt;

  pt = opaque;

  screen_fill_rect_value(pt->scr,
                         pt->ox + x - size / 2,
                         pt->oy + y - size / 2,
                         SIZE2D(size, size),
                         pt->pixels[palette_index]);
}

/* ----------------------------------------------------------------------- */

result_t particles_create(wuss_t *wuss, particles_task_t **out)
{
  result_t          rc;
  particles_task_t *task;
  wuss_task_t      *delegate;
  wuss_task_desc_t  delegate_desc;
  int               i;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;
  task->bg   = colour_rgb(0x00, 0x00, 0x00);
  rng_seed(&task->rng, (uint32_t) rand());

  particles_ramp(particles_firey,  &task->palette[PALETTE_SIZE * PARTICLES_FIREY]);
  particles_ramp(particles_smokey, &task->palette[PALETTE_SIZE * PARTICLES_SMOKEY]);
  particles_ramp(particles_fleck,  &task->palette[PALETTE_SIZE * PARTICLES_FLECK]);
  particles_ramp(particles_pastel, &task->palette[PALETTE_SIZE * PARTICLES_PASTEL]);

  particles_init_styles(task->styles);
  init_particle_system(&task->ps,
                       0,
                       task->styles,
                       PARTICLES_NSTYLES,
                       0.2f,
                       particles_rand,
                       particles_time,
                       particles_render,
                       task);
  task->ps.width  = PARTICLES_WIDTH;
  task->ps.height = PARTICLES_HEIGHT;

  /* particles_redraw paints its own background every frame */
  delegate_desc.handle    = particles_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "particles";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = task_window_create(delegate,
                          SIZE2D(PARTICLES_WIDTH, PARTICLES_HEIGHT),
                          "Particles",
                          &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, PARTICLES_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in
                                * particles_mouse */

  for (i = 0; i < NELEMS(task->emitter_items); i++)
    WUSS_MENU_ITEM(task->emitter_items, i, particles_intensities[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->emitter_menu, "Intensity", task->emitter_items,
                 NELEMS(task->emitter_items));

  WUSS_MENU_ITEM_MENU(task->add_items, PARTICLES_ADD_EMITTER, "Emitter",
                      wuss_MENU_ITEM_NONE, &task->emitter_menu);
  WUSS_MENU_ITEM(task->add_items, PARTICLES_ADD_ATTRACTOR, "Attractor",
                wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);
  WUSS_MENU_ITEM(task->add_items, PARTICLES_ADD_REPELLER, "Repeller",
                wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN);

  WUSS_MENU_TITLE(task->add_menu, "Add", task->add_items,
                 NELEMS(task->add_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, PARTICLES_MENU_ADD, "Add",
                      wuss_MENU_ITEM_NONE, &task->add_menu);

  WUSS_MENU_ITEM_MENU(task->menu_items, PARTICLES_MENU_BACKGROUND, "Background",
                      wuss_MENU_ITEM_PRE_OPEN, wuss_colourmenu_menu(wuss));

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PARTICLES_MENU_PAUSE, "Pause",
                          wuss_MENU_ITEM_NONE, "SPACE");

  task->gravity = PARTICLES_GRAVITY_NORMAL;
  for (i = 0; i < NELEMS(task->gravity_items); i++)
    WUSS_MENU_ITEM(task->gravity_items, i, particles_gravities[i].name,
                   wuss_MENU_ITEM_NONE);

  WUSS_MENU_TITLE(task->gravity_menu, "Gravity", task->gravity_items,
                 NELEMS(task->gravity_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, PARTICLES_MENU_GRAVITY, "Gravity",
                      wuss_MENU_ITEM_NONE, &task->gravity_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PARTICLES_MENU_WALLS, "Walls",
                          wuss_MENU_ITEM_NONE, "W");

  task->markers = 1;
  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PARTICLES_MENU_MARKERS, "Markers",
                          wuss_MENU_ITEM_NONE, "M");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, PARTICLES_MENU_CLEAR, "Clear",
                          wuss_MENU_ITEM_NONE, "C");

  WUSS_MENU_TITLE(task->menu, "Particles", task->menu_items,
                 NELEMS(task->menu_items));

  rc = particles_strength_dialogue_create(task);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregisters; its QUIT frees the task
                                  * block, its window too */
    return rc;
  }

  task->add_items[PARTICLES_ADD_ATTRACTOR].window =
    wuss_dialogue_window(task->strength_dialogue);
  task->add_items[PARTICLES_ADD_REPELLER].window =
    wuss_dialogue_window(task->strength_dialogue);

  if (out)
    *out = task;

  return result_OK;
}

void particles_destroy(particles_task_t *task)
{
  wuss_dialogue_destroy(task->strength_dialogue);
  free(task);
}

/* stack items for the Strength dialogue's layout: label/slider/value-echo
 * rows above a Cancel/Apply button row, as saturn's g_saturn_conf_stack.
 * Labels and values are size-grouped (widths patched from the font in
 * particles_strength_dialogue_create); a spacer in the label group puts the
 * buttons under the sliders. */
enum
{
  PST_ROOT,

  PST_ROW,
  PST_LABL,
  PST_SLDR,
  PST_VAL,

  PST_ROW2,
  PST_LABL2,
  PST_SLDR2,
  PST_VAL2,

  PST_BTNS,
  PST_BSPC,
  PST_BBOX,
  PST_CNCL,
  PST_APLY,

  PARTICLES_STRENGTH_STACK__LIMIT
};

#define PST_G_LABEL      1 /* size groups, see stack_item_t::group */
#define PST_G_VALUE      2
#define PST_SLIDER_MIN_W (64)
#define PST_CHAR_W        6 /* ponytail: assumes the 6px system font */
#define PST_ACTION_WIDTH(W)  ((W)*PST_CHAR_W+2*wuss_STD_SECONDARY_BUTTON_BORDER)
#define PST_DEFAULT_WIDTH(W) ((W)*PST_CHAR_W+2*wuss_STD_PRIMARY_BUTTON_BORDER)
#define PST_CANCEL_W     PST_ACTION_WIDTH(9)
#define PST_APPLY_W      PST_DEFAULT_WIDTH(9)

static const stack_item_t g_particles_strength_stack[PARTICLES_STRENGTH_STACK__LIMIT] =
{
  [PST_ROOT] = { .kind = stack_KIND_VBOX, .parent = -1,
               .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },

  [PST_ROW]  = STACK_HBOX(PST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [PST_LABL] = STACK_LEAF_GROUP(PST_ROW, 0, 16, stack_ALIGN_CENTRE, PST_G_LABEL),
  [PST_SLDR] = STACK_LEAF_EX(PST_ROW, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, PST_SLIDER_MIN_W, 0),
  [PST_VAL]  = STACK_LEAF_GROUP(PST_ROW, 0, 16, stack_ALIGN_CENTRE, PST_G_VALUE),

  [PST_ROW2]  = STACK_HBOX(PST_ROOT, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [PST_LABL2] = STACK_LEAF_GROUP(PST_ROW2, 0, 16, stack_ALIGN_CENTRE, PST_G_LABEL),
  [PST_SLDR2] = STACK_LEAF_EX(PST_ROW2, 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, PST_SLIDER_MIN_W, 0),
  [PST_VAL2]  = STACK_LEAF_GROUP(PST_ROW2, 0, 16, stack_ALIGN_CENTRE, PST_G_VALUE),

  [PST_BTNS] = STACK_HBOX(PST_ROOT, wuss_STD_PRIMARY_BUTTON_HEIGHT, wuss_STD_GAP, stack_ALIGN_START),
  [PST_BSPC] = STACK_SPACER_GROUP(PST_BTNS, 0, PST_G_LABEL),
  [PST_BBOX] = { .kind = stack_KIND_HBOX, .parent = PST_BTNS,
               .gap = wuss_STD_GAP, .flex = 1 },
  [PST_CNCL] = STACK_LEAF_EX(PST_BBOX, 0, wuss_STD_SECONDARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE, 1, PST_CANCEL_W, 0),
  [PST_APLY] = STACK_LEAF_EX(PST_BBOX, 0, wuss_STD_PRIMARY_BUTTON_HEIGHT, stack_ALIGN_CENTRE, 1, PST_APPLY_W, 0),
};

/* Build the Strength dialogue once: a label, a slider 1..100, a label
 * echoing its current value, and Cancel/Apply buttons, positioned by
 * stack_solve. Created hidden -- wuss shows and hides it itself, as a
 * borrowed window shared by both the Attractor and Repeller menu leaves
 * (see task->add_items[PARTICLES_ADD_ATTRACTOR/REPELLER].window), so it
 * must outlive the open menu chain and is never closed here, only
 * hidden. */
static result_t particles_strength_dialogue_create(particles_task_t *task)
{
  result_t         rc;
  wuss_icon_spec_t specs[PARTICLES_STRENGTH_NICONS];
  wuss_icon_t     *made[PARTICLES_STRENGTH_NICONS];
  stack_item_t     items[PARTICLES_STRENGTH_STACK__LIMIT];
  box_t            boxes[PARTICLES_STRENGTH_STACK__LIMIT];
  box_t            root;
  char             value_buf[WUSS_SLIDER_ROW_BUF];
  char             range_buf[WUSS_SLIDER_ROW_BUF];
  char             widest[WUSS_SLIDER_ROW_BUF];
  size2d_t         min_sz;

  /* each label/value's natural width; the size groups widen the rest to match */
  memcpy(items, g_particles_strength_stack, sizeof(items));
  items[PST_LABL].axis_size  = task_text_width(task->wuss, "Strength");
  items[PST_LABL2].axis_size = task_text_width(task->wuss, "Range");
  snprintf(widest, sizeof(widest), "%d", PARTICLES_STRENGTH_MAX);
  items[PST_VAL].axis_size   = task_text_width(task->wuss, widest);
  snprintf(widest, sizeof(widest), "%d", PARTICLES_RANGE_MAX);
  items[PST_VAL2].axis_size  = task_text_width(task->wuss, widest);

  rc = stack_smallest(items, NELEMS(items), &min_sz);
  if (rc != result_OK)
    return rc;

  rc = wuss_dialogue_create(&task->strength_dialogue, task->delegate, min_sz,
                            "Strength", particles_strength_fillout, task);
  if (rc != result_OK)
    return rc;

  root = (box_t) BOX_POS_SIZE(0, 0, min_sz.w, min_sz.h);
  rc = stack_solve(items, NELEMS(items), &root, boxes);
  if (rc != result_OK)
    goto exit;

  wuss_icon_spec_label(&specs[PARTICLES_STRENGTH_ICON_LABEL], boxes[PST_LABL],
                       "Strength", wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_slider_row(&specs[PARTICLES_STRENGTH_ICON_SLIDER],
                            &specs[PARTICLES_STRENGTH_ICON_VALUE],
                            boxes[PST_SLDR], boxes[PST_VAL],
                            wuss_SLIDER_HORIZONTAL,
                            PARTICLES_STRENGTH_MIN, PARTICLES_STRENGTH_MAX,
                            PARTICLES_STRENGTH_DEFAULT, NULL, 0,
                            value_buf, sizeof(value_buf));

  wuss_icon_spec_label(&specs[PARTICLES_STRENGTH_ICON_RANGE_LABEL],
                       boxes[PST_LABL2], "Range",
                       wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_slider_row(&specs[PARTICLES_STRENGTH_ICON_RANGE_SLIDER],
                            &specs[PARTICLES_STRENGTH_ICON_RANGE_VALUE],
                            boxes[PST_SLDR2], boxes[PST_VAL2],
                            wuss_SLIDER_HORIZONTAL,
                            PARTICLES_RANGE_MIN, PARTICLES_RANGE_MAX,
                            PARTICLES_RANGE_DEFAULT, NULL, 0,
                            range_buf, sizeof(range_buf));

  wuss_icon_spec_action(&specs[PARTICLES_STRENGTH_ICON_CANCEL], boxes[PST_CNCL], "Cancel", 0);
  wuss_icon_spec_action(&specs[PARTICLES_STRENGTH_ICON_APPLY], boxes[PST_APLY], "Apply", 1);

  rc = wuss_icon_create_array(wuss_dialogue_window(task->strength_dialogue),
                              specs, PARTICLES_STRENGTH_NICONS, made);
  if (rc != result_OK)
    goto exit;

  wuss_slider_row_bind(&task->strength_row,
                       made[PARTICLES_STRENGTH_ICON_SLIDER],
                       made[PARTICLES_STRENGTH_ICON_VALUE], NULL,
                       PARTICLES_STRENGTH_MIN, PARTICLES_STRENGTH_MAX, 0);
  wuss_slider_row_bind(&task->range_row,
                       made[PARTICLES_STRENGTH_ICON_RANGE_SLIDER],
                       made[PARTICLES_STRENGTH_ICON_RANGE_VALUE], NULL,
                       PARTICLES_RANGE_MIN, PARTICLES_RANGE_MAX, 0);
  task->strength_cancel = made[PARTICLES_STRENGTH_ICON_CANCEL];
  task->strength_apply  = made[PARTICLES_STRENGTH_ICON_APPLY];

  {
    wuss_dialogue_action_t actions[2];

    actions[0].icon = task->strength_cancel;
    actions[0].fn   = particles_strength_cancel;
    actions[1].icon = task->strength_apply;
    actions[1].fn   = particles_strength_apply_action;

    rc = wuss_dialogue_set_actions(task->strength_dialogue, actions,
                                   NELEMS(actions));
    if (rc != result_OK)
      goto exit;
  }

  return result_OK;


exit:
  wuss_dialogue_destroy(task->strength_dialogue); /* not yet a menu leaf: safe to close */
  task->strength_dialogue = NULL;
  return rc;
}

/* Dialogue fillout callback: reset the slider and its echo label to
 * PARTICLES_STRENGTH_DEFAULT. Called by wuss_dialogue_handle_pre_show on
 * every reveal, and directly by particles_strength_cancel to reset the
 * dialogue on an Adjust-Cancel click. */
static result_t particles_strength_fillout(void *opaque)
{
  result_t          rc;
  particles_task_t *task;

  task = opaque;

  rc = wuss_slider_row_set(wuss_dialogue_window(task->strength_dialogue),
                           &task->strength_row, PARTICLES_STRENGTH_DEFAULT);
  if (rc != result_OK)
    return rc;

  return wuss_slider_row_set(wuss_dialogue_window(task->strength_dialogue),
                             &task->range_row, PARTICLES_RANGE_DEFAULT);
}

/* Adds the attractor/repeller at the slider's current value, scaled to a
 * force strength and negated for an attractor, shared by a Select and an
 * Adjust click on Apply. */
static void particles_strength_apply(particles_task_t *task)
{
  float strength;

  strength = wuss_icon_get_value(task->strength_row.slider) *
            PARTICLES_STRENGTH_SCALE / PARTICLES_STRENGTH_MAX;
  if (task->strength_which == PARTICLES_ADD_ATTRACTOR)
    strength = -strength;

  create_repeller(&task->ps, task->menu_x, task->menu_y, strength,
                  (float) wuss_icon_get_value(task->range_row.slider));
}

/* Dialogue action callback for Cancel, split by button per the RISC OS
 * "Adjust doesn't dismiss" convention: Select dismisses the menu chain;
 * Adjust resets the dialogue to the default strength instead. */
static result_t particles_strength_cancel(void *opaque, wuss_button_t button)
{
  particles_task_t *task;

  task = opaque;

  if (button & wuss_BUTTON_SELECT)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
    return result_OK;
  }
  if (button & wuss_BUTTON_ADJUST)
    return particles_strength_fillout(task);
  return result_OK;
}

/* Dialogue action callback for Apply: Select applies and dismisses; Adjust
 * applies but leaves the dialogue open. */
static result_t particles_strength_apply_action(void         *opaque,
                                                wuss_button_t button)
{
  particles_task_t *task;

  task = opaque;

  if (button & wuss_BUTTON_SELECT)
  {
    wuss_menu_close(task->menu_handle);
    task->menu_handle = NULL;
    particles_strength_apply(task);
    return result_OK;
  }
  if (button & wuss_BUTTON_ADJUST)
    particles_strength_apply(task);
  return result_OK;
}

/* wuss_EVENT_ICON on the Strength dialogue: slider drag updates the echo
 * label live on DOWN/MOVE, handled here directly; a Cancel/Apply click (UP
 * only) is dispatched through the dialogue's action table. */
static result_t particles_strength_dialogue_icon(particles_task_t   *task,
                                                 const wuss_event_t *event)
{
  result_t rc;

  if (wuss_slider_row_event(wuss_dialogue_window(task->strength_dialogue),
                            &task->strength_row, event, NULL))
    return result_OK;

  if (wuss_slider_row_event(wuss_dialogue_window(task->strength_dialogue),
                            &task->range_row, event, NULL))
    return result_OK;

  if (wuss_dialogue_handle_icon(task->strength_dialogue, event, &rc))
    return rc;

  return result_OK;
}

static result_t particles_redraw(const wuss_event_t *event, void *task_data)
{
  particles_task_t *pt;
  const box_t      *content, *bounds;
  int               i;

  pt = task_data;

  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  pt->scr = event->data.redraw.scr;
  pt->ox  = bounds->x0 - event->data.redraw.scroll.x;
  pt->oy  = bounds->y0 - event->data.redraw.scroll.y;

  /* resolve the palette once per redraw rather than once per particle; the
   * screen's own palette can change between redraws */
  for (i = 0; i < (int) NELEMS(pt->pixels); i++)
    pt->pixels[i] = screen_colour_to_pixel(pt->scr, pt->palette[i]);

  screen_fill_rect(pt->scr, content->x0, content->y0, box_size(content),
                   pt->bg);

  render_particles(&pt->ps);

  /* green ring round each emitter, red round each repeller and orange round
   * each attractor (a repeller of negative strength), the latter two at
   * their range of influence */
  for (i = 0; pt->markers && i < MAX_EMITTERS; i++)
  {
    if (pt->ps.emitters[i].active)
      screen_draw_circle(pt->scr,
                         pt->ox + (int) pt->ps.emitters[i].x,
                         pt->oy + (int) pt->ps.emitters[i].y,
                         PARTICLES_EMITTER_RING,
                         colour_rgb(0x00, 0xFF, 0x00));

    if (pt->ps.repellers[i].active)
      screen_draw_circle(pt->scr,
                         pt->ox + (int) pt->ps.repellers[i].x,
                         pt->oy + (int) pt->ps.repellers[i].y,
                         (int) pt->ps.repellers[i].max_distance,
                         pt->ps.repellers[i].strength < 0.0f ?
                           colour_rgb(0xFF, 0x99, 0x00) :
                           colour_rgb(0xFF, 0x00, 0x00));
  }

  pt->scr = NULL;

  return result_OK;
}

/* note the pointer's position and velocity for particles_trail */
static void particles_track(particles_task_t *pt, int x, int y)
{
  float dt;

  dt = (pt->now_ms - pt->last_move_ms) / 1000.0f;
  if (pt->pointer_in && dt > 0.0f)
  {
    pt->mvx = (x - pt->mx) / dt;
    pt->mvy = (y - pt->my) / dt;
  }
  else if (!pt->pointer_in)
  {
    /* first move since entering: no velocity yet, and no backlog of trail
     * particles owed for the time spent outside */
    pt->mvx          = 0.0f;
    pt->mvy          = 0.0f;
    pt->last_emit_ms = pt->now_ms;
    pt->pointer_in   = 1;
  }

  pt->mx           = x;
  pt->my           = y;
  pt->last_move_ms = pt->now_ms;
}

/* emit the pointer trail, carrying some of the pointer's velocity */
static void particles_trail(particles_task_t *pt)
{
  float emit_dt;
  int   n;

  if (!pt->pointer_in)
    return;

  emit_dt = (pt->now_ms - pt->last_emit_ms) / 1000.0f;
  n       = CLAMP((int) (emit_dt * PARTICLES_TRAIL_RATE), 0,
                  PARTICLES_TRAIL_MAX);
  if (n == 0)
    return;

  while (n-- > 0)
    create_particle(&pt->ps, PARTICLES_PASTEL, pt->mx, pt->my,
                    pt->mvx * PARTICLES_TRAIL_DAMPING,
                    pt->mvy * PARTICLES_TRAIL_DAMPING);
  pt->last_emit_ms = pt->now_ms;
}

static result_t particles_mouse(wuss_window_t      *window,
                                wuss_mouse_action_t action,
                                int                 x,
                                int                 y,
                                wuss_button_t       button,
                                void               *task_data)
{
  particles_task_t *pt;

  pt = task_data;

  if (window != pt->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  /* x,y arrive in virtual content space, as the particles are held */
  if (action == wuss_MOUSE_MOVE)
  {
    particles_track(pt, x, y);
    return result_OK;
  }

  if (action != wuss_MOUSE_DOWN)
    return result_OK;

  if (button & wuss_BUTTON_MENU)
  {
    static const wuss_proginfo_desc_t desc =
    {
      "Particles",
      "Retro explosion particle system",
      "© David Thomas",
      "1.0 (" __DATE__ ")"
    };
    wuss_proginfo_set_desc(&desc);
    pt->menu_items[PARTICLES_MENU_INFO].window =
      wuss_proginfo_window(pt->delegate);

    pt->menu_x = x;
    pt->menu_y = y;

    wuss_menu_tick_item(&pt->menu, PARTICLES_MENU_PAUSE, pt->paused);
    wuss_menu_tick_exclusive(&pt->gravity_menu, pt->gravity);
    wuss_menu_tick_item(&pt->menu, PARTICLES_MENU_WALLS,
                        !!(pt->ps.flags & PARTICLE_FLAG_WALLS));
    wuss_menu_tick_item(&pt->menu, PARTICLES_MENU_MARKERS, pt->markers);

    return wuss_menu_open_at_pointer(pt->delegate, &pt->menu,
                                     &pt->menu_handle);
  }

  if (button & wuss_BUTTON_SELECT)
    create_explosion(&pt->ps, -1, x, y, 0.0f, 0.0f, PARTICLES_BURST);
  else if (button & wuss_BUTTON_ADJUST)
    create_explosion(&pt->ps, PARTICLES_FLECK, x, y, 0.0f, 0.0f,
                     PARTICLES_BURST);

  return result_OK;
}

/* a Background pick sets the fill; a "Gravity" pick sets the strength; an
 * "Add > Emitter" pick adds a steady smoke emitter, as Explosion's
 * playground sets up, at the menu's opening point; "Add > Repeller" and
 * "Add > Attractor" instead hover-open the shared Strength dialogue (see
 * particles_strength_apply_action), which adds a repeller there at the
 * applied strength, negated for an attractor */
/* A "Gravity" pick: rebuild the styles from scratch, then scale each one's
 * own gravity, so strengths never compound. Particles already in flight pick
 * up the change on the next physics step, as the engine reads gravity from
 * the style each step. */
static void particles_set_gravity(particles_task_t   *pt,
                                  const wuss_event_t *event)
{
  int i;

  pt->gravity = event->data.menu_select.index;

  particles_init_styles(pt->styles);
  for (i = 0; i < PARTICLES_NSTYLES; i++)
    pt->styles[i].gravity *= particles_gravities[pt->gravity].scale;

  wuss_menu_tick_exclusive_live(pt->menu_handle, &pt->gravity_menu,
                                pt->gravity);
}

static result_t particles_menu_select(particles_task_t   *pt,
                                      const wuss_event_t *event)
{
  int index;

  if (wuss_colourmenu_selected_rgb(event, &pt->bg))
    return result_OK; /* the next idle tick repaints */

  if (event->data.menu_select.menu == &pt->gravity_menu)
  {
    particles_set_gravity(pt, event);
    return result_OK;
  }

  if (event->data.menu_select.menu != &pt->emitter_menu)
    return result_OK;

  index = event->data.menu_select.index;
  if (index < 0 || index >= NELEMS(particles_intensities))
    return result_OK;

  create_emitter(&pt->ps, pt->menu_x, pt->menu_y,
                 particles_intensities[index].rate,
                 0.5f, /* jitter */
                 0.0f, /* clump: none */
                 PARTICLES_SMOKEY,
                 0); /* lifetime: forever */

  return result_OK;
}

static result_t particles_idle(void *task_data)
{
  particles_task_t *pt;

  pt = task_data;

  /* the proginfo dialogue is a second window on this same (autoclose)
   * delegate, so closing the main window alone never empties task->windows
   * and the task lingers until the dialogue closes too -- guard against the
   * dangling window in the meantime */
  if (pt->window == NULL)
    return result_OK;

  if (pt->paused)
    return result_OK;

  /* ps.width/height stay the fixed PARTICLES_WIDTH/HEIGHT doc size set in
   * particles_create -- the window's own content box is only the current
   * (possibly scrolled/resized) viewport onto that doc, not the simulation
   * bounds particles die against. */

  /* ponytail: one fixed physics step per idle tick, so speed follows the
   * frame rate; feed a real clock's delta here if that ever matters */
  pt->now_ms += 1000 / PHYSICS_FPS;
  update_particles(&pt->ps, 1.0f / PHYSICS_FPS);
  particles_trail(pt);

  if (!is_active(&pt->ps) && pt->ps.width > 0 && pt->ps.height > 0)
    create_explosion(&pt->ps, -1,
                     (int) (particles_rand(16, pt) % pt->ps.width),
                     (int) (particles_rand(16, pt) % pt->ps.height),
                     0.0f, 0.0f, PARTICLES_BURST);

  wuss_window_invalidate_visible(pt->window);

  return result_OK;
}

/* The "Pause" and "Walls" rows: Pause stops or restarts the idle animation;
 * Walls flips the engine's own flag, which takes effect on the next physics
 * step. An ADJUST pick keeps the menu open, so retick the
 * live row; a SELECT pick has already closed it. */
static result_t particles_toggle(particles_task_t   *pt,
                                 const wuss_event_t *event)
{
  int index;
  int ticked;

  index = event->data.menu_select.index;

  switch (index)
  {
  case PARTICLES_MENU_PAUSE:
    pt->paused = !pt->paused;
    ticked = pt->paused;
    break;

  case PARTICLES_MENU_WALLS:
    pt->ps.flags ^= PARTICLE_FLAG_WALLS;
    ticked = !!(pt->ps.flags & PARTICLE_FLAG_WALLS);
    break;

  case PARTICLES_MENU_MARKERS:
    pt->markers = !pt->markers;
    ticked = pt->markers;
    wuss_window_invalidate_visible(pt->window); /* repaint if paused */
    break;

  default:
    return result_OK;
  }

  wuss_menu_tick_item_live(pt->menu_handle, &pt->menu, index, ticked);

  return result_OK;
}

/* drops every particle and emitter; repaint in case we're paused */
static void particles_clear(particles_task_t *pt)
{
  reset_particle_system(&pt->ps);
  wuss_window_invalidate_visible(pt->window);
}

result_t particles_handle(wuss_window_t      *window,
                          const wuss_event_t *event,
                          void               *task_data)
{
  particles_task_t *pt;

  pt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return particles_redraw(event, task_data);

  case wuss_EVENT_MOUSE:
    return particles_mouse(window, event->data.mouse.action,
                           event->data.mouse.point.x,
                           event->data.mouse.point.y,
                           event->data.mouse.button, task_data);

  case wuss_EVENT_IDLE:
    return particles_idle(task_data);

  case wuss_EVENT_POINTER_EXIT:
    if (window == pt->window)
      pt->pointer_in = 0;
    return result_OK;

  case wuss_EVENT_KEY:
    if (window != pt->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */
    return wuss_menu_dispatch_shortcut(pt->delegate, &pt->menu, event);

  case wuss_EVENT_PRE_SUBMENU_OPEN:
    /* the "Background" row: the shared colourmenu, set up per open */
    return wuss_colourmenu_open_rgb(pt->wuss, event, "Background", pt->bg);

  case wuss_EVENT_ICON:
    if (window == wuss_dialogue_window(pt->strength_dialogue))
      return particles_strength_dialogue_icon(pt, event);
    return result_OK;

  case wuss_EVENT_MENU_SELECT:
    if (event->data.menu_select.menu == &pt->menu &&
        event->data.menu_select.index == PARTICLES_MENU_CLEAR)
    {
      particles_clear(pt);
      return result_OK;
    }
    if (event->data.menu_select.menu == &pt->menu)
      return particles_toggle(pt, event);
    return particles_menu_select(pt, event);

  case wuss_EVENT_MENU_CLOSED:
    pt->menu_handle = NULL;
    return result_OK;

  case wuss_EVENT_CLOSE:
    if (window == pt->window)
      pt->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == wuss_dialogue_window(pt->strength_dialogue))
    {
      pt->strength_which = event->data.pre_show.index;
      rc = wuss_dialogue_handle_pre_show(pt->strength_dialogue);
    }
    else if (window == pt->menu_items[PARTICLES_MENU_INFO].window)
    {
      rc = wuss_proginfo_handle_pre_show();
    }
    else
    {
      rc = result_OK;
    }
    if (rc != result_OK)
      return rc;
    if (event->data.pre_show.handle == NULL)
      return result_OK;
    return wuss_menu_open_window_now(event->data.pre_show.handle,
                                     event->data.pre_show.index);
  }

  case wuss_EVENT_QUIT:
    particles_destroy(pt);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
