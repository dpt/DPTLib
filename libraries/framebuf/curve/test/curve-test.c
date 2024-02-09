/* curve-test.c */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "io/path.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/curve.h"

/* ----------------------------------------------------------------------- */

/* PICO-8 palette */

#define palette_BLACK        (0)
#define palette_DARK_BLUE    (1)
#define palette_DARK_PURPLE  (2)
#define palette_DARK_GREEN   (3)
#define palette_BROWN        (4)
#define palette_DARK_GREY    (5)
#define palette_LIGHT_GREY   (6)
#define palette_WHITE        (7)
#define palette_RED          (8)
#define palette_ORANGE       (9)
#define palette_YELLOW      (10)
#define palette_GREEN       (11)
#define palette_BLUE        (12)
#define palette_LAVENDER    (13)
#define palette_PINK        (14)
#define palette_LIGHT_PEACH (15)

/* ----------------------------------------------------------------------- */

#ifdef USE_SDL

#include <SDL.h>

typedef struct sdlstate
{
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *texture;
}
sdlstate_t;

static void PrintEvent(const SDL_Event *event)
{
  if (event->type == SDL_WINDOWEVENT)
  {
    switch (event->window.event)
    {
    case SDL_WINDOWEVENT_SHOWN:
      SDL_Log("Window %d shown", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_HIDDEN:
      SDL_Log("Window %d hidden", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      SDL_Log("Window %d exposed", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_MOVED:
      SDL_Log("Window %d moved to %d,%d",
              event->window.windowID, event->window.data1,
              event->window.data2);
      break;
    case SDL_WINDOWEVENT_RESIZED:
      SDL_Log("Window %d resized to %dx%d",
              event->window.windowID, event->window.data1,
              event->window.data2);
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      SDL_Log("Window %d size changed to %dx%d",
              event->window.windowID, event->window.data1,
              event->window.data2);
      break;
    case SDL_WINDOWEVENT_MINIMIZED:
      SDL_Log("Window %d minimized", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_MAXIMIZED:
      SDL_Log("Window %d maximized", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_RESTORED:
      SDL_Log("Window %d restored", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_ENTER:
      SDL_Log("Mouse entered window %d",
              event->window.windowID);
      break;
    case SDL_WINDOWEVENT_LEAVE:
      SDL_Log("Mouse left window %d", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      SDL_Log("Window %d gained keyboard focus",
              event->window.windowID);
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      SDL_Log("Window %d lost keyboard focus",
              event->window.windowID);
      break;
    case SDL_WINDOWEVENT_CLOSE:
      SDL_Log("Window %d closed", event->window.windowID);
      break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
    case SDL_WINDOWEVENT_TAKE_FOCUS:
      SDL_Log("Window %d is offered a focus", event->window.windowID);
      break;
    case SDL_WINDOWEVENT_HIT_TEST:
      SDL_Log("Window %d has a special hit test", event->window.windowID);
      break;
#endif
    default:
      SDL_Log("Window %d got unknown event %d",
              event->window.windowID, event->window.event);
      break;
    }
  }
}

static result_t start_sdl(sdlstate_t *state, int width, int height)
{
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    fprintf(stderr, "Error: SDL_Init: %s\n", SDL_GetError());
    goto failure;
  }

  state->window = SDL_CreateWindow("DPTLib SDL Test",
                                   SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   width, height,
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (state->window == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateWindow: %s\n", SDL_GetError());
    goto failure;
  }

  state->renderer = SDL_CreateRenderer(state->window,
                                       -1,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (state->renderer == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateRenderer: %s\n", SDL_GetError());
    goto failure;
  }

  state->texture = SDL_CreateTexture(state->renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width, height);
  if (state->texture == NULL)
  {
    fprintf(stderr, "Error: SDL_CreateTexture: %s\n", SDL_GetError());
    goto failure;
  }

  if (SDL_SetTextureBlendMode(state->texture, SDL_BLENDMODE_NONE) < 0)
  {
    fprintf(stderr, "Error: SDL_SetTextureBlendMode: %s\n", SDL_GetError());
    goto failure;
  }

  return result_OK;


failure:
  return result_NOT_SUPPORTED; /* not ideal */
}

static void stop_sdl(sdlstate_t *state)
{
  if (state == NULL)
    return;

  SDL_DestroyTexture(state->texture);
  SDL_DestroyRenderer(state->renderer);
  SDL_DestroyWindow(state->window);

  SDL_Quit();
}

#endif /* SDL */

/* ----------------------------------------------------------------------- */

#define BLOBSZ        (8)
#define NSETS         (5)
#define MAXCONTROLPTS (2 + 3 + 4 + 5 + 6)
#define MINDRAWPTS    (2)
#define MAXDRAWPTS    (128)
#define BORDER        (64)
#define UNIT          (96)

/* ----------------------------------------------------------------------- */

static const struct
{
  enum
  {
    Linear,
    Quadratic,
    Cubic,
    Quartic,
    Quintic
  }
  kind;
  int offset;
}
curves[] =
{
  { Linear,     0                 },
  { Quadratic,  0 + 2             },
  { Cubic,      0 + 2 + 3         },
  { Quartic,    0 + 2 + 3 + 4     },
  { Quintic,    0 + 2 + 3 + 4 + 5 },
};

/* ----------------------------------------------------------------------- */

typedef struct curveteststate
{
  int         scr_width, scr_height;
  bitmap_t    bm;
  screen_t    scr;
  colour_t    palette[16];
  colour_t    transparent;
  int         background_colour_index;
#ifdef USE_SDL
  sdlstate_t  sdl_state;
#endif

  box_t     overalldirty;

  int       section_height;

  point_t   control_points[MAXCONTROLPTS];
  point_t   draw_points[MAXDRAWPTS];
  int       ndrawpoints;

  float     line_rotation; /* degrees */

  bool      opt_use_aa;
  bool      opt_checker;
  bool      opt_draw_endpoints;
  point_t (*jitterfn)(point_t);
}
curveteststate_t;

/* ----------------------------------------------------------------------- */

static point_t jitter_on(point_t p)
{
  point_t q;

  q.x = p.x + (rand() % 2) - 1;
  q.y = p.y + (rand() % 2) - 1;

  return q;
}

static point_t jitter_off(point_t p)
{
  return p;
}

/* ----------------------------------------------------------------------- */

static result_t setup_all_curves(curveteststate_t *state)
{
  int cpi;
  int y;
  int npoints;
  int set;

  cpi     = 0; /* control point index */
  y       = 0;
  npoints = 2;
  for (set = 0; set < NSETS; set++)
  {
    int width;
    int lefthand;
    int i;

    width    = (npoints - 1) * UNIT;
    lefthand = (state->scr_width - width) / 2;

    for (i = 0; i < npoints; i++)
    {
      state->control_points[cpi].x = lefthand + i * UNIT;
      state->control_points[cpi].y = (i > 0 && i < npoints - 1) ? y : y + UNIT;
      cpi++;
    }

    y += state->section_height + BLOBSZ;
    npoints++;
  }

  return result_OK;
}

/* draw control point blobs */
static result_t draw_all_ctrlpts(curveteststate_t *state)
{
  int cpi;
  int npoints;
  int set;

  cpi     = 0;
  npoints = 2;
  for (set = 0; set < NSETS; set++)
  {
    int i;

    for (i = 0; i < npoints; i++)
    {
      box_t    b;
      colour_t colour;

      b.x0 = state->control_points[cpi].x - BLOBSZ / 2;
      b.y0 = state->control_points[cpi].y - BLOBSZ / 2;
      b.x1 = state->control_points[cpi].x + BLOBSZ / 2;
      b.y1 = state->control_points[cpi].y + BLOBSZ / 2;

      colour = state->palette[8 + set];

      screen_draw_square(&state->scr, b.x0, b.y0, BLOBSZ, colour);

      box_union(&b, &state->overalldirty, &state->overalldirty);

      cpi++;
    }
    npoints++;
  }

  return result_OK;
}

/* draw curve from draw_points[] */
static result_t draw_a_curve(curveteststate_t *state)
{
  int i;

  for (i = 0; i < state->ndrawpoints - 1; i++)
  {
    box_t    b;
    colour_t colour;

    b.x0 = state->draw_points[i + 0].x;
    b.y0 = state->draw_points[i + 0].y;
    b.x1 = state->draw_points[i + 1].x;
    b.y1 = state->draw_points[i + 1].y;

    if (state->opt_checker)
      colour = state->palette[(i & 1) ? palette_DARK_PURPLE : palette_LIGHT_PEACH];
    else
      colour = state->palette[palette_BLACK];

    if (state->opt_draw_endpoints)
    {
      screen_draw_pixel(&state->scr, b.x0, b.y0, state->palette[palette_GREEN]);
      screen_draw_pixel(&state->scr, b.x1, b.y1, state->palette[palette_RED]);
    }

    if (state->opt_use_aa)
      screen_draw_aa_line(&state->scr, b.x0, b.y0, b.x1, b.y1, colour);
    else
      screen_draw_line(&state->scr, b.x0, b.y0, b.x1, b.y1, colour);

    box_union(&b, &state->overalldirty, &state->overalldirty);
  }

  return result_OK;
}

/* calculate curves */
static result_t calc_all_curves(curveteststate_t *state)
{
  int set;

  for (set = 0; set < NELEMS(curves); set++)
  {
    int o = curves[set].offset;
    int i;

    for (i = 0; i < state->ndrawpoints; i++)
    {
      int t = 65536 * i / (state->ndrawpoints - 1);

      switch (curves[set].kind)
      {
      case Linear:
        state->draw_points[i] = curve_point_on_line(state->jitterfn(state->control_points[o + 0]),
                                                    state->jitterfn(state->control_points[o + 1]),
                                                    t);
        break;

      case Quadratic:
        state->draw_points[i] = curve_bezier_point_on_quad(state->jitterfn(state->control_points[o + 0]),
                                                           state->jitterfn(state->control_points[o + 1]),
                                                           state->jitterfn(state->control_points[o + 2]),
                                                           t);
        break;

      case Cubic:
        state->draw_points[i] = curve_bezier_point_on_cubic(state->jitterfn(state->control_points[o + 0]),
                                                            state->jitterfn(state->control_points[o + 1]),
                                                            state->jitterfn(state->control_points[o + 2]),
                                                            state->jitterfn(state->control_points[o + 3]),
                                                            t);
        break;

      case Quartic:
        state->draw_points[i] = curve_bezier_point_on_quartic(state->jitterfn(state->control_points[o + 0]),
                                                              state->jitterfn(state->control_points[o + 1]),
                                                              state->jitterfn(state->control_points[o + 2]),
                                                              state->jitterfn(state->control_points[o + 3]),
                                                              state->jitterfn(state->control_points[o + 4]),
                                                              t);
        break;

      case Quintic:
        state->draw_points[i] = curve_bezier_point_on_quintic(state->jitterfn(state->control_points[o + 0]),
                                                              state->jitterfn(state->control_points[o + 1]),
                                                              state->jitterfn(state->control_points[o + 2]),
                                                              state->jitterfn(state->control_points[o + 3]),
                                                              state->jitterfn(state->control_points[o + 4]),
                                                              state->jitterfn(state->control_points[o + 5]),
                                                              t);
        break;
      }
    }

    (void) draw_a_curve(state);
  }

  return result_OK;
}

/* rotate a line or three */
static result_t rotate_lines(curveteststate_t *state)
{
  const float centre   = 40.0f;
  const float diameter = 21.0f;

  float s, c;
  float xa, ya, xb, yb;
  box_t b = BOX_RESET;

  xa = centre + sinf((state->line_rotation +   0.0f) / 360.0f * M_PI * 2.0f) * diameter;
  ya = centre + cosf((state->line_rotation +   0.0f) / 360.0f * M_PI * 2.0f) * diameter;
  xb = centre + sinf((state->line_rotation + 180.0f) / 360.0f * M_PI * 2.0f) * diameter;
  yb = centre + cosf((state->line_rotation + 180.0f) / 360.0f * M_PI * 2.0f) * diameter;

  screen_draw_line(&state->scr, (int) xa, (int) ya, (int) xb, (int) yb, state->palette[palette_DARK_GREEN]);
  screen_draw_aa_line(&state->scr, xa + 45, ya, xb + 45, yb, state->palette[palette_DARK_GREEN]);
  screen_draw_aa_linef(&state->scr, xa + 90, ya, xb + 90, yb, state->palette[palette_DARK_GREEN]);

  box_extend_n(&b, 2, 0, 0, 150, 150);
  box_union(&b, &state->overalldirty, &state->overalldirty); // this will leave trails since it's not wiping the /old/ box
}

static result_t curve_interactive_test(curveteststate_t *state)
{
  bool  quit          = false;
  int   frame;
  int   mx            = 0;
  int   my            = 0;
  int   firstdraw     = 1;
  int   aa            = 1;
  int   cycling       = 1;
  bool  opt_dontclear = false;
  int   points        = 1;
  box_t prevdirty     = BOX_RESET;
  int   dragging      = -1;
  int   i;
  int   set;

  state->opt_use_aa         = true;
  state->opt_checker        = true;
  state->opt_draw_endpoints = true;

  state->ndrawpoints        = 32;
  state->overalldirty       = (box_t) BOX_RESET;
  state->jitterfn           = &jitter_off;

  state->section_height     = (state->scr_height - (NSETS - 1) * BLOBSZ) / NSETS; /* divide screen into chunks */

  setup_all_curves(state);

  for (frame = 0; !quit; frame++)
  {
#ifdef USE_SDL
    {
      SDL_Event event;

      /* Consume all pending events */
      while (SDL_PollEvent(&event))
      {
        switch (event.type)
        {
        case SDL_QUIT:
          quit = 1;
          SDL_Log("Quitting after %i ticks", event.quit.timestamp);
          break;

        case SDL_WINDOWEVENT:
          PrintEvent(&event);
          break;

        case SDL_KEYDOWN:
          break;

        case SDL_KEYUP:
          switch (event.key.keysym.sym)
          {
          case SDLK_a: state->opt_use_aa = !state->opt_use_aa; break;
          case SDLK_c: state->opt_checker = !state->opt_checker; break;
          case SDLK_j: state->jitterfn = (state->jitterfn == jitter_on) ? &jitter_off : &jitter_on; break;
          case SDLK_p: state->opt_draw_endpoints = !state->opt_draw_endpoints; break;
          case SDLK_q: quit = true; break;
          }
          break;

        case SDL_MOUSEMOTION:
          mx = event.motion.x;
          my = event.motion.y;

          state->line_rotation = mx / 4.0f;

          if (dragging >= 0) {
            state->control_points[dragging].x = mx;
            state->control_points[dragging].y = my;
          }
          break;

        case SDL_MOUSEBUTTONDOWN:
          mx = event.motion.x;
          my = event.motion.y;

          switch (event.button.button)
          {
          case SDL_BUTTON_LEFT:
            for (i = 0; i < NELEMS(state->control_points); i++)
            {
              box_t point_box;

              point_box.x0 = state->control_points[i].x - BLOBSZ / 2;
              point_box.y0 = state->control_points[i].y - BLOBSZ / 2;
              point_box.x1 = state->control_points[i].x + BLOBSZ / 2;
              point_box.y1 = state->control_points[i].y + BLOBSZ / 2;

              if (box_contains_point(&point_box, mx, my))
              {
                dragging = i;
                state->control_points[dragging].x = mx;
                state->control_points[dragging].y = my;
                break;
              }
            }
            break;

          case SDL_BUTTON_RIGHT:
            opt_dontclear = true;
            break;
          }
          break;

        case SDL_MOUSEBUTTONUP:
          switch (event.button.button)
          {
          case SDL_BUTTON_LEFT:
            dragging = -1;
            break;

          case SDL_BUTTON_RIGHT:
            opt_dontclear = false;
            break;
          }
          break;

        case SDL_MOUSEWHEEL:
          if (event.motion.yrel > 0)
            state->ndrawpoints = MIN(state->ndrawpoints + 1, MAXDRAWPTS);
          else if (event.motion.yrel < 0)
            state->ndrawpoints = MAX(state->ndrawpoints - 1, MINDRAWPTS);
          break;

        default:
          printf("Unhandled event code {%d}\n", event.type);
          break;
        }
      }
    }
#else
    if (frame > 1000)
      quit = 1;
#endif

    if (!opt_dontclear)
    {
      bitmap_clear(&state->bm, state->palette[state->background_colour_index]);
      box_reset(&state->overalldirty);
    }

    (void) draw_all_ctrlpts(state);
    (void) calc_all_curves(state);
    (void) rotate_lines(state);

#ifdef USE_SDL
    if (!box_is_empty(&state->overalldirty))
    {
      /* Update the texture and render it */

      bitmap_t *scr_bgrx8888;
      SDL_Rect  texturearea;
      char     *scr;
      box_t     scr_clip;
      box_t     combined_dirty;

      if (state->scr.format != pixelfmt_bgrx8888)
        /* convert the screen to bgrx8888 */
        bitmap_convert((const bitmap_t *) &state->scr,
                                           pixelfmt_bgrx8888,
                                          &scr_bgrx8888);
      else
        scr_bgrx8888 = (bitmap_t *) &state->scr;

      (void) screen_get_clip(&state->scr, &scr_clip);

      if (firstdraw)
      {
        firstdraw = 0;
        combined_dirty = scr_clip;
      }
      else
      {
        box_union(&state->overalldirty, &prevdirty, &combined_dirty);
        (void) box_intersection(&scr_clip, &combined_dirty, &combined_dirty);
      }

      texturearea.x = combined_dirty.x0;
      texturearea.y = combined_dirty.y0;
      texturearea.w = combined_dirty.x1 - combined_dirty.x0;
      texturearea.h = combined_dirty.y1 - combined_dirty.y0;

      /* the rect passed here says where to draw in the texture */
      scr = (char *) scr_bgrx8888->base +
                     texturearea.y * scr_bgrx8888->rowbytes +
                     texturearea.x * 4;
      SDL_UpdateTexture(state->sdl_state.texture,
                       &texturearea,
                        scr,
                        scr_bgrx8888->rowbytes);
    }

    /* Render texture */
    SDL_RenderCopy(state->sdl_state.renderer, state->sdl_state.texture, NULL, NULL);
    SDL_RenderPresent(state->sdl_state.renderer);

    SDL_Delay(1000 / 60); /* 60fps */
#endif

    prevdirty = state->overalldirty;
  }

#ifdef USE_SDL
  stop_sdl(&state->sdl_state);
#endif

  return result_TEST_PASSED;
}

static void define_pico8_palette(colour_t palette[16])
{
  palette[palette_BLACK      ] = colour_rgb(0x00, 0x00, 0x00);
  palette[palette_DARK_BLUE  ] = colour_rgb(0x1D, 0x2B, 0x53);
  palette[palette_DARK_PURPLE] = colour_rgb(0x7E, 0x25, 0x53);
  palette[palette_DARK_GREEN ] = colour_rgb(0x00, 0x87, 0x51);
  palette[palette_BROWN      ] = colour_rgb(0xAB, 0x52, 0x36);
  palette[palette_DARK_GREY  ] = colour_rgb(0x5F, 0x57, 0x4F);
  palette[palette_LIGHT_GREY ] = colour_rgb(0xC2, 0xC3, 0xC7);
  palette[palette_WHITE      ] = colour_rgb(0xFF, 0xF1, 0xE8);
  palette[palette_RED        ] = colour_rgb(0xFF, 0x00, 0x4D);
  palette[palette_ORANGE     ] = colour_rgb(0xFF, 0xA3, 0x00);
  palette[palette_YELLOW     ] = colour_rgb(0xFF, 0xEC, 0x27);
  palette[palette_GREEN      ] = colour_rgb(0x00, 0xE4, 0x36);
  palette[palette_BLUE       ] = colour_rgb(0x29, 0xAD, 0xFF);
  palette[palette_LAVENDER   ] = colour_rgb(0x83, 0x76, 0x9C);
  palette[palette_PINK       ] = colour_rgb(0xFF, 0x77, 0xA8);
  palette[palette_LIGHT_PEACH] = colour_rgb(0xFF, 0xCC, 0xAA);
}

result_t curve_test_one_format(const char *resources,
                               int         scr_width,
                               int         scr_height,
                               pixelfmt_t  scr_fmt)
{
  curveteststate_t state;

  state.scr_width  = scr_width;
  state.scr_height = scr_height;


  const int scr_rowbytes = (state.scr_width << pixelfmt_log2bpp(scr_fmt)) / 8;

  define_pico8_palette(&state.palette[0]);

  state.transparent = colour_rgba(0x00, 0x00, 0x00, 0x00);

  state.background_colour_index = 7; /* near white */

  result_t      rc = result_OK;
  unsigned int *pixels;
  int           bm_inited = 0;
  int           font;

#ifdef USE_SDL
  memset(&state.sdl_state, 0, sizeof(state.sdl_state));

  rc = start_sdl(&state.sdl_state, state.scr_width, state.scr_height);
  if (rc)
    goto Failure;
#endif

  pixels = malloc(scr_rowbytes * state.scr_height);
  if (pixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }

  bitmap_init(&state.bm,
               state.scr_width,
               state.scr_height,
               scr_fmt,
               scr_rowbytes,
               state.palette,
               pixels);
  bm_inited = 1;

  bitmap_clear(&state.bm, state.palette[state.background_colour_index]);

  screen_for_bitmap(&state.scr, &state.bm);

  /* ------------------------------------------------------------------------ */

  typedef result_t (*bmfonttestfn_t)(curveteststate_t *);

  static const bmfonttestfn_t tests[] =
  {
    curve_interactive_test
  };

  int test;

  for (test = 0; test < NELEMS(tests); test++)
  {
    rc = tests[test](&state);
    if (rc != result_TEST_PASSED)
      goto Failure;
  }

  /* ------------------------------------------------------------------------ */

Cleanup:
  if (bm_inited)
    free(state.bm.base);

  return rc;


Failure:
  fprintf(stderr, "error: %x\n", rc);
  rc = result_TEST_FAILED;
  goto Cleanup;
}

result_t curve_test(const char *resources)
{
  static const struct
  {
    int        width, height;
    pixelfmt_t fmt;
    int        log2bpp;
  }
  tab[] =
  {
    //    { 800, 600, pixelfmt_p4       },
    { 800, 600, pixelfmt_bgrx8888 }
  };

  result_t rc;
  int      i;

  for (i = 0; i < NELEMS(tab); i++)
  {
    rc = curve_test_one_format(resources,
                               tab[i].width,
                               tab[i].height,
                               tab[i].fmt);
    if (rc != result_TEST_PASSED)
      return rc;
  }

  return rc;
}

/* vim: set ts=8 sts=2 sw=2 et: */
