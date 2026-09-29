/* wuss/test/tasks/spheroid.c -- KPT Spheroid Designer-style lit sphere */

#ifdef WUSS_APP

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "geom/stack.h"
#include "wuss/icon.h"
#include "wuss/task.h"

#include "spheroid.h"
#include "common.h"

/* After Kai's Power Tools 3's Spheroid Designer (MetaTools, 1995): one big
 * sphere lit by up to four coloured lights, with knobs for the highlight's
 * size and sharpness, a rim glow and an ambient floor.
 *
 * Each pixel inside the sphere's disc takes its surface normal N straight
 * from its position (the sphere is viewed orthographically, so the viewer
 * direction V is always +z). Per light L:
 *
 *   diffuse  = max(0, N.L) * light colour * sphere colour
 *   specular = smoothstep(max(0, N.H) ^ e) * light colour, H = |L + V|
 *
 * where the highlight size picks the exponent e and its sharpness narrows
 * the smoothstep around 0.5. Glow adds (1 - N.V)^3 of the sphere colour at
 * the rim and ambient a flat share of it. The sum is clamped, then blended
 * over the background by the pixel's disc coverage so the edge is
 * anti-aliased.
 *
 * A light is placed by pointing at the sphere: inside the disc it takes the
 * surface normal under the pointer. A band SPHEROID_WRAP radii wide just
 * outside the disc wraps round to the back hemisphere, z falling from 0 at
 * the rim to -1 at the band's outer edge, so rim and back lighting stay in
 * reach. Markers use the inverse, so a back light's marker sits in the band
 * where the pointer that placed it was. */

#define SPHEROID_STRIP  176  /* control strip width, px, at the left */
#define SPHEROID_MARGIN 8    /* px between the wrap band and the window edge */
#define SPHEROID_WRAP   0.25 /* wrap band width, in sphere radii */
#define SPHEROID_MARKER 4    /* marker ring radius, px */
#define SPHEROID_GRAB   6    /* px from a marker a click selects it */
#define SPHEROID_NUDGE  1    /* px an arrow key moves the current light */
#define SPHEROID_SHOVE  8    /* px a Shift-arrow moves it */

#define SPHEROID_DITHER_TILE 64 /* px; the blue-noise map's size */

#define SPHEROID_SAVE_NAME "spheroid.png" /* written to the current dir */

#define SPHEROID_MUTATE_SLIDER 15   /* max slider nudge, % of its range */
#define SPHEROID_MUTATE_LIGHT  0.35 /* max direction nudge, ~sin(20 deg) */
#define SPHEROID_MUTATE_HUE    0.2  /* max hue turn, radians */

enum
{
  SPHEROID_MENU_INFO = 0,
  SPHEROID_MENU_LIGHT,
  SPHEROID_MENU_DITHERING,
  SPHEROID_MENU_MUTATE,
  SPHEROID_MENU_RANDOMISE,
  SPHEROID_MENU_RESET,
  SPHEROID_MENU_SAVE
};

/* the strip's colour sets */
enum
{
  SPHEROID_COLOUR_SPHERE,
  SPHEROID_COLOUR_BACKGROUND,
  SPHEROID_COLOUR_LIGHT /* the current light's */
};

/* control strip rows, top to bottom */
enum
{
  SPHEROID_ROW_AMBIENT,
  SPHEROID_ROW_GLOW,
  SPHEROID_ROW_SIZE,      /* this and the rest are the current light's */
  SPHEROID_ROW_SHARPNESS,
  SPHEROID_ROW_INTENSITY /* the current light's */
};

static const struct
{
  const char *label;
  int         max;
  const char *fmt;
}
spheroid_rows[SPHEROID_NROWS] =
{
  { "Ambient", 100, NULL   },
  { "Glow",    100, NULL   },
  { "Size",    100, NULL   },
  { "Sharp",   100, NULL   },
  { "Level",   200, "%d%%" }
};

/* the Light frame's caption and the Light submenu's rows */
static const char *const spheroid_light_names[SPHEROID_NLIGHTS] =
{
  "Light 1", "Light 2", "Light 3", "Light 4"
};

/* stack items for the control strip: a VBOX of three frames, the
 * background's colour set above the sphere's settings above the current
 * light's, the latter two a VBOX of label / slider / value rows; the Sphere
 * frame starts with its colour set, the Light frame with an On option and the
 * light's colour set, each on its own line. A box lays out its children in
 * index order, so these lines come before the rows */
enum
{
  SS_ROOT,
  SS_BACKDROP, /* frame round the background's colour set */
  SS_SPHERE,   /* frame round the global rows */
  SS_LIGHT,    /* frame round the per-light rows */
  SS_ON_LINE,  /* the On label and option, then a spacer taking the rest */
  SS_ON_LABEL,
  SS_ON,
  SS_ON_SPACER,
  SS_COLOUR,   /* the colour sets, in SPHEROID_COLOUR_* order */
  SS_ROW = SS_COLOUR + SPHEROID_NCOLOURS, /* first row; each row is SS_ROW +
                                           * 4 * n, then its three leaves */
  SS__LIMIT = SS_ROW + 4 * SPHEROID_NROWS
};

/* icons in the strip: the three frames, label / slider / value per row,
 * then the On option */
enum
{
  SI_BACKDROP,
  SI_SPHERE,
  SI_LIGHT,
  SI_ROW,
  SI_ON_LABEL = SI_ROW + 3 * SPHEROID_NROWS,
  SI_ON,
  SI__LIMIT
};

#define SS_G_LABEL      1 /* size groups, see stack_item_t::group */
#define SS_G_VALUE      2
#define SS_SLIDER_MIN_W (64)

#define SS_FRAME(parent_) \
  { .kind = stack_KIND_VBOX, .parent = (parent_), .axis_size = STACK_HUG, \
    .gap = wuss_STD_GAP, .align = stack_ALIGN_FILL, \
    .pad = wuss_STD_FRAME_INSETS }

/* a full-width line: a colour set */
#define SS_LINE(frame) \
  STACK_LEAF(frame, wuss_STD_SECONDARY_BUTTON_HEIGHT, 0, stack_ALIGN_FILL)

#define SS_ROW_ITEMS(n, frame) \
  [SS_ROW + 4 * (n)]     = STACK_HBOX(frame, wuss_STD_SLIDER_HEIGHT, wuss_STD_GAP, stack_ALIGN_START), \
  [SS_ROW + 4 * (n) + 1] = STACK_LEAF_GROUP(SS_ROW + 4 * (n), 0, 16, stack_ALIGN_CENTRE, SS_G_LABEL), \
  [SS_ROW + 4 * (n) + 2] = STACK_LEAF_EX(SS_ROW + 4 * (n), 0, wuss_STD_SLIDER_HEIGHT, stack_ALIGN_CENTRE, 1, SS_SLIDER_MIN_W, 0), \
  [SS_ROW + 4 * (n) + 3] = STACK_LEAF_GROUP(SS_ROW + 4 * (n), 0, 16, stack_ALIGN_CENTRE, SS_G_VALUE)

static const stack_item_t spheroid_strip[SS__LIMIT] =
{
  [SS_ROOT]     = { .kind = stack_KIND_VBOX, .parent = -1,
                    .gap = wuss_STD_GAP, .pad = wuss_STD_INSETS },
  [SS_BACKDROP] = SS_FRAME(SS_ROOT),
  [SS_SPHERE]   = SS_FRAME(SS_ROOT),
  [SS_LIGHT]    = SS_FRAME(SS_ROOT),
  SS_ROW_ITEMS(SPHEROID_ROW_AMBIENT,   SS_SPHERE),
  SS_ROW_ITEMS(SPHEROID_ROW_GLOW,      SS_SPHERE),
  SS_ROW_ITEMS(SPHEROID_ROW_SIZE,      SS_LIGHT),
  SS_ROW_ITEMS(SPHEROID_ROW_SHARPNESS, SS_LIGHT),
  SS_ROW_ITEMS(SPHEROID_ROW_INTENSITY, SS_LIGHT),
  [SS_ON_LINE]   = STACK_HBOX(SS_LIGHT, wuss_STD_OPTION_SIZE, wuss_STD_GAP, stack_ALIGN_FILL),
  [SS_ON_LABEL]  = STACK_LEAF_GROUP(SS_ON_LINE, 0, 16, stack_ALIGN_CENTRE, SS_G_LABEL),
  [SS_ON]        = STACK_LEAF(SS_ON_LINE, wuss_STD_OPTION_SIZE, 0, stack_ALIGN_FILL),
  [SS_ON_SPACER] = STACK_SPACER(SS_ON_LINE, 1),
  [SS_COLOUR + SPHEROID_COLOUR_SPHERE]     = SS_LINE(SS_SPHERE),
  [SS_COLOUR + SPHEROID_COLOUR_BACKGROUND] = SS_LINE(SS_BACKDROP),
  [SS_COLOUR + SPHEROID_COLOUR_LIGHT]      = SS_LINE(SS_LIGHT)
};

/* Copies spheroid_strip into items, giving each label and value its natural
 * width in the desktop font; the size groups widen the rest to match. */
static void spheroid_strip_items(const wuss_t *wuss, stack_item_t *items)
{
  char text[WUSS_SLIDER_ROW_BUF];
  int  row;

  memcpy(items, spheroid_strip, sizeof(spheroid_strip));

  for (row = 0; row < SPHEROID_NROWS; row++)
  {
    items[SS_ROW + 4 * row + 1].axis_size =
      task_text_width(wuss, spheroid_rows[row].label);

    snprintf(text, sizeof(text),
             spheroid_rows[row].fmt ? spheroid_rows[row].fmt : "%d",
             spheroid_rows[row].max);
    items[SS_ROW + 4 * row + 3].axis_size = task_text_width(wuss, text);
  }

  items[SS_ON_LABEL].axis_size = task_text_width(wuss, "On");
}

/* per-redraw constants, worked out once rather than per pixel */
typedef struct spheroid_frame
{
  double sphere[3];     /* surface colour, 0..1 */
  double background[3]; /* 0..1 */
  int    nlights;       /* only the lights that are on */
  double l[SPHEROID_NLIGHTS][3]; /* light direction */
  double h[SPHEROID_NLIGHTS][3]; /* Blinn half-vector, |L + V| */
  double c[SPHEROID_NLIGHTS][3]; /* colour * intensity, 0..2 */
  double exponent[SPHEROID_NLIGHTS];
  double lo[SPHEROID_NLIGHTS], hi[SPHEROID_NLIGHTS]; /* specular smoothstep
                                                      * edges */
  double ambient;
  double glow;
}
spheroid_frame_t;

/* ----------------------------------------------------------------------- */

static void spheroid_rgb(colour_t c, double scale, double out[3])
{
  unsigned int r, g, b;

  colour_get_rgb(&c, &r, &g, &b);
  out[0] = r / 255.0 * scale;
  out[1] = g / 255.0 * scale;
  out[2] = b / 255.0 * scale;
}

/* alpha and rgb, 0..1, as a bgra8888 pixel: 0xAARRGGBB, straight alpha */
static unsigned int spheroid_pack(double a, const double rgb[3])
{
  return ((unsigned int) (a      * 255.0 + 0.5) << 24) |
         ((unsigned int) (rgb[0] * 255.0 + 0.5) << 16) |
         ((unsigned int) (rgb[1] * 255.0 + 0.5) << 8)  |
         ((unsigned int) (rgb[2] * 255.0 + 0.5));
}

static void spheroid_set_light(spheroid_light_t *light,
                               int               on,
                               double            x,
                               double            y,
                               double            z,
                               colour_t          colour,
                               int               intensity)
{
  double len;

  len = sqrt(x * x + y * y + z * z);

  light->on        = on;
  light->x         = x / len;
  light->y         = y / len;
  light->z         = z / len;
  light->colour    = colour;
  light->intensity = intensity;
}

static void spheroid_defaults(spheroid_task_t *task)
{
  int i;

  spheroid_set_light(&task->lights[0], 1, -0.5,  0.6,  0.6,
                     colour_rgb(0xFF, 0xFF, 0xFF), 100);
  spheroid_set_light(&task->lights[1], 1,  0.7, -0.6, -0.3,
                     colour_rgb(0x40, 0x60, 0xFF), 60);
  spheroid_set_light(&task->lights[2], 0,  0.6,  0.6,  0.5,
                     colour_rgb(0xFF, 0x80, 0x40), 100);
  spheroid_set_light(&task->lights[3], 0, -0.6, -0.6,  0.5,
                     colour_rgb(0x40, 0xFF, 0x80), 100);

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    task->lights[i].size      = 40;
    task->lights[i].sharpness = 30;
  }

  task->sphere     = colour_rgb(0x80, 0x80, 0x80);
  task->background = colour_rgb(0x00, 0x00, 0x00);
  task->ambient    = 10;
  task->glow       = 15;
}

static void spheroid_prepare(const spheroid_task_t *task,
                             spheroid_frame_t      *f)
{
  const spheroid_light_t *light;
  double                  len, w;
  int                     i, n;

  spheroid_rgb(task->sphere, 1.0, f->sphere);
  spheroid_rgb(task->background, 1.0, f->background);

  n = 0;
  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    light = &task->lights[i];
    if (!light->on)
      continue;

    f->l[n][0] = light->x;
    f->l[n][1] = light->y;
    f->l[n][2] = light->z;

    /* L + V with V = (0,0,1); only degenerate for a light dead behind */
    len = sqrt(light->x * light->x + light->y * light->y +
               (light->z + 1.0) * (light->z + 1.0));
    len = MAX(len, 1e-6);
    f->h[n][0] = light->x / len;
    f->h[n][1] = light->y / len;
    f->h[n][2] = (light->z + 1.0) / len;

    spheroid_rgb(light->colour, light->intensity / 100.0, f->c[n]);

    /* 200..2 */
    f->exponent[n] = 2.0 * pow(100.0, (100 - light->size) / 100.0);

    w        = 1.0 - light->sharpness * 0.0098; /* 1..0.02 */
    f->lo[n] = 0.5 - w / 2.0;
    f->hi[n] = 0.5 + w / 2.0;
    n++;
  }
  f->nlights = n;

  f->ambient = task->ambient / 100.0;
  f->glow    = task->glow / 50.0;
}

/* the sphere's centre and radius for a content area of the given size, in
 * content-local coordinates: centred in the pane right of the strip */
static void spheroid_layout(size2d_t size, double *cx, double *cy, double *r)
{
  int w;

  w   = MAX(size.w - SPHEROID_STRIP, 0);
  *cx = SPHEROID_STRIP + w / 2.0;
  *cy = size.h / 2.0;
  *r  = (MIN(w, size.h) / 2.0 - SPHEROID_MARGIN) / (1.0 + SPHEROID_WRAP);
}

/* the task field a strip row edits, taking a per-light row's from the given
 * light */
static int *spheroid_field(spheroid_task_t *task, int row, int light)
{
  switch (row)
  {
  case SPHEROID_ROW_AMBIENT:   return &task->ambient;
  case SPHEROID_ROW_GLOW:      return &task->glow;
  case SPHEROID_ROW_SIZE:      return &task->lights[light].size;
  case SPHEROID_ROW_SHARPNESS: return &task->lights[light].sharpness;
  default:                     return &task->lights[light].intensity;
  }
}

/* the task field a strip row edits now */
static int *spheroid_row_field(spheroid_task_t *task, int row)
{
  return spheroid_field(task, row, task->current);
}

/* repaint the preview pane only, leaving the strip's icons alone */
static void spheroid_invalidate_preview(spheroid_task_t *task)
{
  box_t content, pane;

  if (task->window == NULL)
    return;

  wuss_window_get_content_bounds(task->window, &content);
  pane = (box_t) BOX_POS_SIZE(SPHEROID_STRIP, 0,
                              MAX(box_size(&content).w - SPHEROID_STRIP, 0),
                              box_size(&content).h);
  wuss_window_invalidate(task->window, &pane);
}

/* point the light at content-local (px, py) */
static void spheroid_aim(spheroid_light_t *light,
                         double            cx,
                         double            cy,
                         double            r,
                         double            px,
                         double            py)
{
  double dx, dy, d, z, s;

  dx = (px - cx) / r;
  dy = (cy - py) / r;
  d  = sqrt(dx * dx + dy * dy);

  if (d <= 1.0)
  {
    light->x = dx;
    light->y = dy;
    light->z = sqrt(1.0 - d * d);
    return;
  }

  z = -MIN((d - 1.0) / SPHEROID_WRAP, 1.0);
  s = sqrt(1.0 - z * z);
  light->x = dx / d * s;
  light->y = dy / d * s;
  light->z = z;
}

/* the inverse of spheroid_aim: where the light's marker sits */
static void spheroid_marker_pos(const spheroid_light_t *light,
                                double                  cx,
                                double                  cy,
                                double                  r,
                                double                 *mx,
                                double                 *my)
{
  double s, d;

  if (light->z >= 0.0)
  {
    *mx = cx + light->x * r;
    *my = cy - light->y * r;
    return;
  }

  s = sqrt(light->x * light->x + light->y * light->y);
  d = (1.0 - light->z * SPHEROID_WRAP) * r;
  if (s < 1e-6)
  {
    /* dead behind: every direction round the band is equally right */
    *mx = cx;
    *my = cy - d;
    return;
  }

  *mx = cx + light->x / s * d;
  *my = cy - light->y / s * d;
}

/* a lit light's marker: a ring in its colour with a black outline for
 * contrast, dotted when behind the sphere, centre filled when current */
static void spheroid_draw_marker(screen_t               *scr,
                                 const spheroid_light_t *light,
                                 int                     x,
                                 int                     y,
                                 int                     current)
{
  colour_t black;
  int      i;

  black = colour_rgb(0x00, 0x00, 0x00);

  if (light->z >= 0.0)
  {
    screen_draw_circle(scr, x, y, SPHEROID_MARKER + 1, black);
    screen_draw_circle(scr, x, y, SPHEROID_MARKER, light->colour);
  }
  else
  {
    for (i = 0; i < 12; i++)
      screen_set_pixel(scr,
                       x + (int) floor(SPHEROID_MARKER * cos(i * M_PI / 6.0) + 0.5),
                       y + (int) floor(SPHEROID_MARKER * sin(i * M_PI / 6.0) + 0.5),
                       light->colour);
  }

  if (current)
    screen_fill_circle(scr, x, y, SPHEROID_MARKER - 2, light->colour);
}

/* shade the surface point with normal n into rgb (0..1, clamped) */
static void spheroid_shade(const spheroid_frame_t *f,
                           const double            n[3],
                           double                  rgb[3])
{
  double ndl, ndh, spec, t, rim;
  int    i, k;

  rim = pow(1.0 - n[2], 3.0) * f->glow;
  for (k = 0; k < 3; k++)
    rgb[k] = f->sphere[k] * (f->ambient + rim);

  for (i = 0; i < f->nlights; i++)
  {
    ndl = n[0] * f->l[i][0] + n[1] * f->l[i][1] + n[2] * f->l[i][2];
    if (ndl <= 0.0)
      continue;

    ndh  = n[0] * f->h[i][0] + n[1] * f->h[i][1] + n[2] * f->h[i][2];
    spec = ndh > 0.0 ? pow(ndh, f->exponent[i]) : 0.0;
    t    = CLAMP((spec - f->lo[i]) / (f->hi[i] - f->lo[i]), 0.0, 1.0);
    spec = t * t * (3.0 - 2.0 * t);

    for (k = 0; k < 3; k++)
      rgb[k] += f->c[i][k] * (ndl * f->sphere[k] + spec);
  }

  for (k = 0; k < 3; k++)
    rgb[k] = CLAMP(rgb[k], 0.0, 1.0);
}

/* the disc coverage and surface normal of the pixel whose centre is (dx, dy)
 * from the sphere's centre; the normal is only set when the coverage is
 * non-zero */
static double spheroid_normal(double dx, double dy, double r, double n[3])
{
  double cov, n2, len;

  cov = CLAMP(r - sqrt(dx * dx + dy * dy) + 0.5, 0.0, 1.0);
  if (cov == 0.0)
    return cov;

  /* screen y runs down, the model's runs up; an edge pixel whose centre lies
   * just outside the disc is pulled back onto the rim */
  n[0] =  dx / r;
  n[1] = -dy / r;
  n2   = n[0] * n[0] + n[1] * n[1];
  if (n2 > 1.0)
  {
    len   = sqrt(n2);
    n[0] /= len;
    n[1] /= len;
    n2    = 1.0;
  }
  n[2] = sqrt(1.0 - n2);

  return cov;
}

/* ----------------------------------------------------------------------- */

/* the field colour set "which" shows and sets */
static colour_t *spheroid_colour_field(spheroid_task_t *task, int which)
{
  switch (which)
  {
  case SPHEROID_COLOUR_SPHERE:     return &task->sphere;
  case SPHEROID_COLOUR_BACKGROUND: return &task->background;
  default:                         return &task->lights[task->current].colour;
  }
}

/* show each colour set's field as its nearest palette entry. ponytail: a
 * Mutated or Randomised colour shows as that approximation until a pick */
static void spheroid_sync_colours(spheroid_task_t *task)
{
  int          which;
  unsigned int r, g, b;

  for (which = 0; which < SPHEROID_NCOLOURS; which++)
  {
    colour_get_rgb(spheroid_colour_field(task, which), &r, &g, &b);
    (void) wuss_colourset_set_colour(task->colour_sets[which],
                                     wuss_nearest_colour(task->wuss,
                                                         (int) r, (int) g,
                                                         (int) b));
  }
}

/* a colour set's pick: its field takes that palette entry */
static result_t spheroid_colour_changed(wuss_colourset_t *colourset,
                                        wuss_colour_t     colour,
                                        void             *opaque)
{
  spheroid_task_t *task;
  const colour_t  *palette;
  int              npalette;
  int              which;

  task    = opaque;
  palette = wuss_get_palette(task->wuss, &npalette);
  for (which = 0; which < SPHEROID_NCOLOURS; which++)
    if (task->colour_sets[which] == colourset)
      *spheroid_colour_field(task, which) = palette[colour];
  spheroid_invalidate_preview(task);
  return result_OK;
}

/* build the control strip's icons down the window's left edge, "height"
 * tall */
static result_t spheroid_strip_create(spheroid_task_t *task, int height)
{
  result_t         rc;
  box_t            root;
  stack_item_t     items[SS__LIMIT];
  box_t            boxes[SS__LIMIT];
  box_t            line;
  wuss_icon_spec_t specs[SI__LIMIT];
  char             bufs[SPHEROID_NROWS][WUSS_SLIDER_ROW_BUF];
  wuss_icon_t     *made[SI__LIMIT];
  int              row, item, icon, which;

  spheroid_strip_items(task->wuss, items);

  root = (box_t) BOX_POS_SIZE(0, 0, SPHEROID_STRIP, height);
  rc = stack_solve(items, NELEMS(items), &root, boxes);
  if (rc != result_OK)
    return rc;

  wuss_icon_spec_frame(&specs[SI_BACKDROP], boxes[SS_BACKDROP], "Background");
  wuss_icon_spec_frame(&specs[SI_SPHERE], boxes[SS_SPHERE], "Sphere");
  wuss_icon_spec_frame(&specs[SI_LIGHT], boxes[SS_LIGHT],
                       spheroid_light_names[task->current]);

  for (row = 0; row < SPHEROID_NROWS; row++)
  {
    item = SS_ROW + 4 * row;
    icon = SI_ROW + 3 * row;
    wuss_icon_spec_label(&specs[icon], boxes[item + 1],
                         spheroid_rows[row].label,
                         wuss_ICON_FLAGS_JUSTIFY_RIGHT);
    wuss_icon_spec_slider_row(&specs[icon + 1], &specs[icon + 2],
                              boxes[item + 2], boxes[item + 3],
                              wuss_SLIDER_HORIZONTAL,
                              0, spheroid_rows[row].max,
                              *spheroid_row_field(task, row),
                              spheroid_rows[row].fmt, 0,
                              bufs[row], sizeof(bufs[row]));
  }

  /* On is a label and a bare glyph so each sits in its column */
  wuss_icon_spec_label(&specs[SI_ON_LABEL], boxes[SS_ON_LABEL], "On",
                       wuss_ICON_FLAGS_JUSTIFY_RIGHT);
  wuss_icon_spec_option(&specs[SI_ON], boxes[SS_ON], "");

  rc = wuss_icon_create_array(task->window, specs, NELEMS(specs), made);
  if (rc != result_OK)
    return rc;

  task->light_frame = made[SI_LIGHT];
  task->on_icon     = made[SI_ON];
  wuss_icon_set_selected(task->window, task->on_icon,
                         task->lights[task->current].on);

  for (which = 0; which < SPHEROID_NCOLOURS; which++)
  {
    /* end the field where the sliders end, the button in the value column */
    line     = boxes[SS_COLOUR + which];
    line.x1  = boxes[SS_ROW + 2].x1 + wuss_STD_GAP;
    line.x1 += wuss_colourset_button_width(task->wuss, line.y1 - line.y0);
    rc = wuss_colourset_create(&task->colour_sets[which], task->window,
                               line, "Colour",
                               boxes[SS_ROW + 1].x1 - boxes[SS_ROW + 1].x0,
                               /* line up with the sliders */
                               0, spheroid_colour_changed, task);
    if (rc != result_OK)
      return rc;
  }
  spheroid_sync_colours(task);

  for (row = 0; row < SPHEROID_NROWS; row++)
  {
    icon = SI_ROW + 3 * row;
    wuss_slider_row_bind(&task->rows[row], made[icon + 1], made[icon + 2],
                         spheroid_rows[row].fmt,
                         0, spheroid_rows[row].max, 0);
  }

  return result_OK;
}

result_t spheroid_create(wuss_t *wuss, spheroid_task_t **out)
{
  result_t         rc;
  spheroid_task_t *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;
  stack_item_t     items[SS__LIMIT];
  size2d_t         strip_sz;
  int              i;

  /* the strip sets the window's minimum height */
  spheroid_strip_items(wuss, items);
  rc = stack_smallest(items, NELEMS(items), &strip_sz);
  if (rc != result_OK)
    return rc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss      = wuss;
  task->dithering = screen_DITHER_BLUE_NOISE;
  spheroid_defaults(task);
  rng_seed(&task->rng, (uint32_t) rand());

  delegate_desc.handle    = spheroid_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "spheroid";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; nobody else owns it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  /* no scrollbars: the sphere is laid out across the visible content, so a
   * resize must redraw all of it and doc is only the growth ceiling. wuss
   * fills the strip's background; the redraw paints the preview pane */
  rc = wuss_window_create_placed(delegate,
                                 SIZE2D(SPHEROID_STRIP + 240,
                                        MAX(240, strip_sz.h)),
                                 "Spheroid Designer",
                                 wuss_WINDOW_CLOSE | wuss_WINDOW_BACK |
                                 wuss_WINDOW_TOGGLE_SIZE | wuss_WINDOW_RESIZE |
                                 wuss_WINDOW_NO_RESIZE_BLIT |
                                 wuss_WINDOW_FOCUSABLE,
                                 wuss_BACKDROP_COLOUR(wuss_COLOUR_WINDOW),
                                 SIZE2D(1024, 1024),
                                 SIZE2D(SPHEROID_STRIP + 64, strip_sz.h),
                                 &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* unregister; its QUIT frees the task block */
    return rc;
  }

  rc = spheroid_strip_create(task, strip_sz.h);
  if (rc != result_OK)
  {
    wuss_window_close(task->window); /* its QUIT frees the task block */
    return rc;
  }

  WUSS_MENU_ITEM_WINDOW(task->menu_items, SPHEROID_MENU_INFO, "Info",
                        wuss_MENU_ITEM_BORROWED_SUBMENU | wuss_MENU_ITEM_PRE_OPEN,
                        NULL); /* retargeted at the shared proginfo singleton
                                * just before wuss_menu_open, in spheroid_mouse */

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    static const char *const keys[SPHEROID_NLIGHTS] =
    {
      "1", "2", "3", "4"
    };

    WUSS_MENU_ITEM_SHORTCUT(task->light_items, i, spheroid_light_names[i],
                            wuss_MENU_ITEM_NONE, keys[i]);
  }

  WUSS_MENU_TITLE(task->light_menu, "Light", task->light_items,
                  NELEMS(task->light_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, SPHEROID_MENU_LIGHT, "Light",
                      wuss_MENU_ITEM_NONE, &task->light_menu);

  /* indexed by screen_dither_t */
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_NONE, "None",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_BAYER, "Bayer",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_ITEM(task->dither_items, screen_DITHER_BLUE_NOISE, "Blue noise",
                 wuss_MENU_ITEM_NONE);
  WUSS_MENU_TITLE(task->dither_menu, "Dithering", task->dither_items,
                  NELEMS(task->dither_items));

  WUSS_MENU_ITEM_MENU(task->menu_items, SPHEROID_MENU_DITHERING, "Dithering",
                      wuss_MENU_ITEM_NONE, &task->dither_menu);

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SPHEROID_MENU_MUTATE, "Mutate",
                          wuss_MENU_ITEM_DASHED, "M");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SPHEROID_MENU_RANDOMISE,
                          "Randomise", wuss_MENU_ITEM_NONE, "^R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SPHEROID_MENU_RESET, "Reset",
                          wuss_MENU_ITEM_NONE, "R");

  WUSS_MENU_ITEM_SHORTCUT(task->menu_items, SPHEROID_MENU_SAVE, "Save PNG",
                          wuss_MENU_ITEM_DASHED, "^S");

  WUSS_MENU_TITLE(task->menu, "Spheroid", task->menu_items,
                  NELEMS(task->menu_items));

  if (out)
    *out = task;

  return result_OK;
}

void spheroid_destroy(spheroid_task_t *task)
{
  int which;

  for (which = 0; which < SPHEROID_NCOLOURS; which++)
    wuss_colourset_destroy(task->colour_sets[which]);
  free(task);
}

/* shade the dirty part of the preview pane into a bgra8888 buffer and blit it
 * dithered. The buffer's top-left is pulled back to a whole number of
 * dither tiles from the pane's corner, so the dither stays locked to the
 * pane across partial redraws; the clip is narrowed to the dirty box so the
 * unshaded margin that adds is never drawn */
static result_t spheroid_paint(screen_t               *scr,
                               screen_dither_t         method,
                               const box_t            *content,
                               const box_t            *bounds,
                               const spheroid_frame_t *f,
                               double                  cx,
                               double                  cy,
                               double                  r)
{
  result_t      rc;
  int           x0, ox, oy, w, h;
  unsigned int *pixels;
  int           x, y, k;
  double        cov;
  double        n[3], rgb[3];
  bitmap_t      bm;
  box_t         saved;

  x0 = MAX(content->x0, bounds->x0 + SPHEROID_STRIP);
  if (x0 >= content->x1 || content->y0 >= content->y1)
    return result_OK;

  ox = x0 - (x0 - bounds->x0 - SPHEROID_STRIP) % SPHEROID_DITHER_TILE;
  oy = content->y0 - (content->y0 - bounds->y0) % SPHEROID_DITHER_TILE;
  w  = content->x1 - ox;
  h  = content->y1 - oy;

  pixels = calloc((size_t) w * h, sizeof(*pixels));
  if (pixels == NULL)
    return result_OOM;

  for (y = content->y0; y < content->y1; y++)
  {
    for (x = x0; x < content->x1; x++)
    {
      cov = spheroid_normal(x + 0.5 - cx, y + 0.5 - cy, r, n);
      if (cov == 0.0)
      {
        pixels[(y - oy) * w + (x - ox)] = spheroid_pack(1.0, f->background);
        continue;
      }

      spheroid_shade(f, n, rgb);

      for (k = 0; k < 3; k++)
        rgb[k] = rgb[k] * cov + f->background[k] * (1.0 - cov);

      pixels[(y - oy) * w + (x - ox)] = spheroid_pack(1.0, rgb);
    }
  }

  rc = bitmap_init(&bm, SIZE2D(w, h), pixelfmt_bgra8888,
                   w * (int) sizeof(*pixels), NULL, pixels);
  if (rc == result_OK)
  {
    saved = scr->clip;
    if (box_intersection(&saved, content, &scr->clip))
      rc = result_OK; /* nothing visible to draw */
    else
      rc = screen_copy_bitmap_dithered(scr, ox, oy, &bm, method);
    scr->clip = saved;
  }

  free(pixels);

  return rc;
}

static result_t spheroid_redraw(const wuss_event_t *event,
                                spheroid_task_t    *task)
{
  result_t         rc;
  screen_t        *scr;
  const box_t     *content, *bounds;
  spheroid_frame_t f;
  double           cx, cy, r;
  int              i;
  double           mx, my;

  scr     = event->data.redraw.scr;
  content = event->data.redraw.content;
  bounds  = event->data.redraw.bounds;

  spheroid_prepare(task, &f);

  spheroid_layout(box_size(bounds), &cx, &cy, &r);
  cx += bounds->x0;
  cy += bounds->y0;

  rc = spheroid_paint(scr, task->dithering, content, bounds, &f, cx, cy, r);
  if (rc != result_OK)
    return rc;

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    if (!task->lights[i].on)
      continue;

    spheroid_marker_pos(&task->lights[i], cx, cy, r, &mx, &my);
    spheroid_draw_marker(scr, &task->lights[i],
                         (int) floor(mx), (int) floor(my),
                         i == task->current);
  }

  return result_OK;
}

/* point the Light frame at the current light: its caption, its sliders, its
 * On option and its colour set (resyncing the other two with it) */
static void spheroid_sync_light(spheroid_task_t *task)
{
  int row;

  if (task->window == NULL)
    return;

  /* ponytail: an OOM here only leaves the caption or a value label stale */
  (void) wuss_icon_set_text(task->window, task->light_frame,
                            spheroid_light_names[task->current]);
  for (row = SPHEROID_ROW_SIZE; row < SPHEROID_NROWS; row++)
    (void) wuss_slider_row_set(task->window, &task->rows[row],
                               *spheroid_row_field(task, row));
  wuss_icon_set_selected(task->window, task->on_icon,
                         task->lights[task->current].on);
  spheroid_sync_colours(task);
}

/* turn the current light on, as moving it does, ticking its On option */
static void spheroid_light_on(spheroid_task_t *task)
{
  task->lights[task->current].on = 1;
  wuss_icon_set_selected(task->window, task->on_icon, 1);
}

/* put every strip control back in step with its field */
static void spheroid_sync_rows(spheroid_task_t *task)
{
  int row;

  if (task->window == NULL)
    return;

  /* ponytail: an OOM here only leaves a value label stale */
  for (row = 0; row < SPHEROID_ROW_SIZE; row++)
    (void) wuss_slider_row_set(task->window, &task->rows[row],
                               *spheroid_row_field(task, row));
  spheroid_sync_light(task);
}

/* a random value in [-1, 1] */
static double spheroid_jitter(rng_t *rng)
{
  return rng_range(rng, 2001) / 1000.0 - 1.0;
}

/* turn the colour's hue by up to SPHEROID_MUTATE_HUE radians: a rotation
 * about the grey axis, which keeps its lightness roughly as it was */
static void spheroid_turn_hue(colour_t *c, rng_t *rng)
{
  double       a, cs, sn, k, rgb[3], out[3];
  unsigned int v[3];
  int          i;

  a  = spheroid_jitter(rng) * SPHEROID_MUTATE_HUE;
  cs = cos(a);
  sn = sin(a);
  k  = (1.0 - cs) / 3.0;

  spheroid_rgb(*c, 1.0, rgb);

  /* Rodrigues' rotation about (1,1,1)/sqrt(3), multiplied out */
  for (i = 0; i < 3; i++)
    out[i] = rgb[i] * (cs + k) +
             rgb[(i + 1) % 3] * (k - sn / sqrt(3.0)) +
             rgb[(i + 2) % 3] * (k + sn / sqrt(3.0));

  for (i = 0; i < 3; i++)
    v[i] = (unsigned int) (CLAMP(out[i], 0.0, 1.0) * 255.0 + 0.5);

  *c = colour_rgb(v[0], v[1], v[2]);
}

/* nudge a strip row's field by up to SPHEROID_MUTATE_SLIDER percent of its
 * range, taking a per-light row's from the given light */
static void spheroid_nudge(spheroid_task_t *task, int row, int light)
{
  int d, *field;

  d      = spheroid_rows[row].max * SPHEROID_MUTATE_SLIDER / 100;
  field  = spheroid_field(task, row, light);
  *field = CLAMP(*field + rng_range(&task->rng, 2 * d + 1) - d,
                 0, spheroid_rows[row].max);
}

/* nudge the global sliders, each lit light's sliders and direction, and the
 * colours' hues; which lights are on is left alone */
static void spheroid_mutate(spheroid_task_t *task)
{
  int               row, i;
  spheroid_light_t *light;
  double            x, y, z;

  for (row = 0; row < SPHEROID_ROW_SIZE; row++)
    spheroid_nudge(task, row, 0);

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    light = &task->lights[i];
    if (!light->on)
      continue;

    for (row = SPHEROID_ROW_SIZE; row < SPHEROID_NROWS; row++)
      spheroid_nudge(task, row, i);

    /* ponytail: an offset then renormalise, not a true bounded rotation;
     * turns are up to ~20 degrees, a little more along the diagonals */
    x = light->x + spheroid_jitter(&task->rng) * SPHEROID_MUTATE_LIGHT;
    y = light->y + spheroid_jitter(&task->rng) * SPHEROID_MUTATE_LIGHT;
    z = light->z + spheroid_jitter(&task->rng) * SPHEROID_MUTATE_LIGHT;
    spheroid_set_light(light, 1, x, y, z, light->colour, light->intensity);
    spheroid_turn_hue(&light->colour, &task->rng);
  }

  spheroid_turn_hue(&task->sphere, &task->rng);
}

/* a colour with each channel picked at random */
static colour_t spheroid_random_colour(rng_t *rng)
{
  unsigned int r, g, b;

  r = (unsigned int) rng_range(rng, 256);
  g = (unsigned int) rng_range(rng, 256);
  b = (unsigned int) rng_range(rng, 256);

  return colour_rgb(r, g, b);
}

/* pick every setting afresh: sliders across their full ranges, which lights
 * are on (at least one), their directions and every colour */
static void spheroid_randomise(spheroid_task_t *task)
{
  int               row, i, lit;
  spheroid_light_t *light;
  double            x, y, z;

  for (row = 0; row < SPHEROID_ROW_SIZE; row++)
    *spheroid_field(task, row, 0) = rng_range(&task->rng,
                                              spheroid_rows[row].max + 1);

  lit = 0;
  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    light = &task->lights[i];

    for (row = SPHEROID_ROW_SIZE; row < SPHEROID_NROWS; row++)
      *spheroid_field(task, row, i) = rng_range(&task->rng,
                                                spheroid_rows[row].max + 1);

    /* a point in the unit ball, away from the centre, gives a direction
     * spread evenly over the sphere */
    do
    {
      x = spheroid_jitter(&task->rng);
      y = spheroid_jitter(&task->rng);
      z = spheroid_jitter(&task->rng);
    }
    while (x * x + y * y + z * z > 1.0 || x * x + y * y + z * z < 0.01);

    spheroid_set_light(light, rng_range(&task->rng, 2), x, y, z,
                       spheroid_random_colour(&task->rng), light->intensity);
    lit |= light->on;
  }

  if (!lit)
    task->lights[rng_range(&task->rng, SPHEROID_NLIGHTS)].on = 1;

  task->sphere     = spheroid_random_colour(&task->rng);
  task->background = spheroid_random_colour(&task->rng);
}

/* write the sphere alone, sized to its current diameter, as an RGBA PNG:
 * alpha is the disc coverage, so the edge stays anti-aliased and everything
 * outside it is transparent */
static result_t spheroid_save(spheroid_task_t *task)
{
  result_t         rc;
  box_t            content;
  spheroid_frame_t f;
  double           cx, cy, r;
  int              d;
  unsigned int    *pixels;
  int              x, y;
  double           cov;
  double           n[3], rgb[3];
  bitmap_t         bm;

  if (task->window == NULL)
    return result_OK;

  wuss_window_get_content_bounds(task->window, &content);
  spheroid_layout(box_size(&content), &cx, &cy, &r);
  if (r < 1.0)
    return result_OK; /* window too small to show a sphere */

  spheroid_prepare(task, &f);

  d = (int) ceil(2.0 * r);
  pixels = calloc((size_t) d * d, sizeof(*pixels));
  if (pixels == NULL)
    return result_OOM;

  for (y = 0; y < d; y++)
  {
    for (x = 0; x < d; x++)
    {
      cov = spheroid_normal(x + 0.5 - d / 2.0, y + 0.5 - d / 2.0, r, n);
      if (cov == 0.0)
        continue; /* calloc left it transparent */

      spheroid_shade(&f, n, rgb);

      pixels[y * d + x] = spheroid_pack(cov, rgb);
    }
  }

  rc = bitmap_init(&bm, SIZE2D(d, d), pixelfmt_bgra8888,
                   d * (int) sizeof(*pixels), NULL, pixels);
  if (rc == result_OK)
    rc = bitmap_save_png(&bm, SPHEROID_SAVE_NAME);
  if (rc != result_OK)
    logf_warning("spheroid: saving \"%s\" failed (rc=0x%X)",
                 SPHEROID_SAVE_NAME, rc);
  else
    logf_info("spheroid: saved \"%s\"", SPHEROID_SAVE_NAME);

  free(pixels);

  return rc;
}

/* the lit light whose marker is within SPHEROID_GRAB px of (px, py), or -1 */
static int spheroid_hit_marker(const spheroid_task_t *task,
                               double                 cx,
                               double                 cy,
                               double                 r,
                               int                    px,
                               int                    py)
{
  int    i;
  double mx, my;

  for (i = 0; i < SPHEROID_NLIGHTS; i++)
  {
    if (!task->lights[i].on)
      continue;

    spheroid_marker_pos(&task->lights[i], cx, cy, r, &mx, &my);
    if ((px - mx) * (px - mx) + (py - my) * (py - my) <=
        SPHEROID_GRAB * SPHEROID_GRAB)
      return i;
  }

  return -1;
}

static result_t spheroid_mouse(spheroid_task_t    *task,
                               wuss_mouse_action_t action,
                               point_t             p,
                               wuss_button_t       button,
                               wuss_window_t      *window)
{
  static const wuss_proginfo_desc_t desc =
    TASK_PROGINFO_DESC("Spheroid Designer",
                       "Lit sphere after Kai's Power Tools");

  box_t  content;
  double cx, cy, r;
  int    hit;

  if (window != task->window)
    return result_OK; /* the proginfo dialogue has no click behaviour of
                       * its own */

  if (action == wuss_MOUSE_UP)
  {
    task->dragging = 0;
    return result_OK;
  }

  if (action == wuss_MOUSE_DOWN && (button & wuss_BUTTON_MENU))
  {
    wuss_proginfo_set_desc(&desc);
    task->menu_items[SPHEROID_MENU_INFO].window = wuss_proginfo_window(task->delegate);
    wuss_menu_tick_exclusive(&task->light_menu, task->current);
    wuss_menu_tick_exclusive(&task->dither_menu, (int) task->dithering);

    return wuss_menu_open_at_pointer(task->delegate, &task->menu,
                                     &task->menu_handle);
  }

  if (action == wuss_MOUSE_DOWN &&
      (!(button & wuss_BUTTON_SELECT) || p.x < SPHEROID_STRIP))
    return result_OK; /* strip clicks land on its sliders, or nowhere */

  if (action == wuss_MOUSE_MOVE && !task->dragging)
    return result_OK;

  wuss_window_get_content_bounds(window, &content);
  spheroid_layout(box_size(&content), &cx, &cy, &r);

  if (action == wuss_MOUSE_DOWN)
  {
    task->dragging = 1;

    hit = spheroid_hit_marker(task, cx, cy, r, p.x, p.y);
    if (hit >= 0)
    {
      task->current = hit; /* grab it where it is; a move then drags it */
      spheroid_sync_light(task);
      spheroid_invalidate_preview(task);
      return result_OK;
    }
  }

  spheroid_light_on(task);
  spheroid_aim(&task->lights[task->current], cx, cy, r, p.x, p.y);
  spheroid_invalidate_preview(task);

  return result_OK;
}

/* O switches the current light, as its On option does; an arrow key moves
 * its marker, turning the light on, as a drag would */
static result_t spheroid_key(spheroid_task_t    *task,
                             const wuss_event_t *event)
{
  box_t  content;
  double cx, cy, r;
  double step;
  double mx, my;
  double dx, dy, d, limit;

  if (event->data.key.modifiers & (wuss_KEY_MOD_CTRL | wuss_KEY_MOD_ALT))
    return result_WUSS_KEY_UNCLAIMED;

  if (event->data.key.code == 'O' || event->data.key.code == 'o')
  {
    task->lights[task->current].on = !task->lights[task->current].on;
    wuss_icon_set_selected(task->window, task->on_icon,
                           task->lights[task->current].on);
    spheroid_invalidate_preview(task);
    return result_OK;
  }

  wuss_window_get_content_bounds(task->window, &content);
  spheroid_layout(box_size(&content), &cx, &cy, &r);

  step = (event->data.key.modifiers & wuss_KEY_MOD_SHIFT) ? SPHEROID_SHOVE
                                                          : SPHEROID_NUDGE;

  spheroid_marker_pos(&task->lights[task->current], cx, cy, r, &mx, &my);

  switch (event->data.key.code)
  {
  case wuss_KEY_LEFT:  mx -= step; break;
  case wuss_KEY_RIGHT: mx += step; break;
  case wuss_KEY_UP:    my -= step; break;
  case wuss_KEY_DOWN:  my += step; break;
  default:             return result_WUSS_KEY_UNCLAIMED;
  }

  /* stop just short of the wrap band's outer edge: there the light is dead
   * behind and its marker would jump to the top of the band */
  dx    = mx - cx;
  dy    = my - cy;
  d     = sqrt(dx * dx + dy * dy);
  limit = (1.0 + SPHEROID_WRAP) * r - 0.5;
  if (d > limit)
  {
    mx = cx + dx / d * limit;
    my = cy + dy / d * limit;
  }

  spheroid_light_on(task);
  spheroid_aim(&task->lights[task->current], cx, cy, r, mx, my);
  spheroid_invalidate_preview(task);

  return result_OK;
}

/* offer "event" to each colour set; non-zero if one consumed it, its result
 * in "out_result" */
static int spheroid_colour_event(spheroid_task_t    *task,
                                 const wuss_event_t *event,
                                 result_t           *out_result)
{
  int which;

  for (which = 0; which < SPHEROID_NCOLOURS; which++)
    if (wuss_colourset_handle_event(task->colour_sets[which], event,
                                    out_result))
      return 1;

  return 0;
}

static result_t spheroid_menu_select(spheroid_task_t    *task,
                                     const wuss_event_t *event)
{
  result_t rc;
  int      index;

  if (spheroid_colour_event(task, event, &rc))
    return rc;

  if (event->data.menu_select.menu == &task->menu)
  {
    switch (event->data.menu_select.index)
    {
    case SPHEROID_MENU_MUTATE:
      spheroid_mutate(task);
      break;

    case SPHEROID_MENU_RANDOMISE:
      spheroid_randomise(task);
      break;

    case SPHEROID_MENU_RESET:
      spheroid_defaults(task);
      break;

    case SPHEROID_MENU_SAVE:
      return spheroid_save(task);

    default:
      return result_OK; /* a pick on a submenu row itself */
    }

    spheroid_sync_rows(task);
    spheroid_invalidate_preview(task);
    return result_OK;
  }

  index = event->data.menu_select.index;

  if (event->data.menu_select.menu == &task->dither_menu)
  {
    task->dithering = (screen_dither_t) index;
    wuss_menu_tick_exclusive_live(task->menu_handle, &task->dither_menu,
                                  index);
    spheroid_invalidate_preview(task);
    return result_OK;
  }

  if (event->data.menu_select.menu != &task->light_menu)
    return result_OK;

  task->current = index;
  spheroid_sync_light(task);

  wuss_menu_tick_exclusive_live(task->menu_handle, &task->light_menu, index);

  spheroid_invalidate_preview(task);

  return result_OK;
}

/* a strip click: the colour set's button, toggle the current light, or a
 * slider drag stores the value; each reshades */
static result_t spheroid_icon(spheroid_task_t    *task,
                              wuss_window_t      *window,
                              const wuss_event_t *event)
{
  result_t rc;
  int      row, value;

  if (spheroid_colour_event(task, event, &rc))
    return rc;

  if (event->data.icon.icon == task->on_icon)
  {
    if (event->data.icon.action != wuss_MOUSE_UP)
      return result_OK;

    task->lights[task->current].on = !!wuss_icon_get_selected(task->on_icon);
    spheroid_invalidate_preview(task);
    return result_OK;
  }

  for (row = 0; row < SPHEROID_NROWS; row++)
  {
    if (!wuss_slider_row_event(window, &task->rows[row], event, &value))
      continue;

    *spheroid_row_field(task, row) = value;
    spheroid_invalidate_preview(task);
    break;
  }

  return result_OK;
}

result_t spheroid_handle(wuss_window_t      *window,
                         const wuss_event_t *event,
                         void               *task_data)
{
  spheroid_task_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_REDRAW:
    return spheroid_redraw(event, task);

  case wuss_EVENT_MOUSE:
    return spheroid_mouse(task, event->data.mouse.action,
                          event->data.mouse.point, event->data.mouse.button,
                          window);

  case wuss_EVENT_KEY:
  {
    result_t rc;

    if (window != task->window)
      return result_WUSS_KEY_UNCLAIMED; /* not the proginfo dialogue */

    rc = wuss_menu_dispatch_shortcut(task->delegate, &task->menu, event);
    if (rc != result_WUSS_KEY_UNCLAIMED)
      return rc;

    return spheroid_key(task, event);
  }

  case wuss_EVENT_MENU_SELECT:
    return spheroid_menu_select(task, event);

  case wuss_EVENT_ICON:
    if (window != task->window)
      return result_OK;
    return spheroid_icon(task, window, event);

  case wuss_EVENT_MENU_CLOSED:
  {
    result_t rc;

    (void) spheroid_colour_event(task, event, &rc);
    task->menu_handle = NULL;
    return result_OK;
  }

  case wuss_EVENT_CLOSE:
    if (window == task->window)
      task->window = NULL;
    return result_OK;

  case wuss_EVENT_PRE_SHOW:
  {
    result_t rc;

    if (window == task->menu_items[SPHEROID_MENU_INFO].window)
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

  case wuss_EVENT_QUIT:
    spheroid_destroy(task);
    return result_OK;

  default:
    return result_OK;
  }
}

#endif /* WUSS_APP */
