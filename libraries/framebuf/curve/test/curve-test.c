/* curve-test.c */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "io/path.h"
#include "framebuf/colour.h"
#include "framebuf/palettes.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/curve.h"

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
#define NSETS         (6)
#define MAXCONTROLPTS (2 + 3 + 4 + 5 + 6 + 4)
#define MINSEGMENTS   (1)
#define MAXSEGMENTS   (128)
#define BORDER        (32)
#define UNIT          (32)

/* ----------------------------------------------------------------------- */

static const struct
{
  enum
  {
    Linear,
    Quadratic,
    Cubic,
    Quartic,
    Quintic,
    FwdDiffCubic
  }
  kind;
  int npoints;
  int offset;
}
curves[] =
{
  { Linear,       2, 0                     },
  { Quadratic,    3, 0 + 2                 },
  { Cubic,        4, 0 + 2 + 3             },
  { Quartic,      5, 0 + 2 + 3 + 4         },
  { Quintic,      6, 0 + 2 + 3 + 4 + 5     },
  { FwdDiffCubic, 4, 0 + 2 + 3 + 4 + 5 + 6 }
};

/* ----------------------------------------------------------------------- */

typedef struct curveteststate
{
  int         scr_width, scr_height;
  bitmap_t    bm;
  screen_t    scr;
  colour_t    palette[16];
  colour_t    transparent;
  int         background_palette_index;
#ifdef USE_SDL
  sdlstate_t  sdl_state;
#endif

  box_t       overalldirty;
  int         section_height;
  int         nsegments;

  struct
  {
    bool      use_aa;
    bool      checker;
    bool      draw_endpoints;
  }
  opt;
  point_t   (*jitterfn)(point_t);

  point_t     control_points[MAXCONTROLPTS];
  point_t     draw_points[MAXSEGMENTS];
}
curveteststate_t;

/* ----------------------------------------------------------------------- */

static result_t curve_basic_test(curveteststate_t *state)
{
  point_t a = {   0,   0 };
  point_t b = { 100, 100 };

  point_t c = curve_point_on_line(a, b, 0);
  assert(c.x == a.x);
  assert(c.y == a.y);

  point_t d = curve_point_on_line(a, b, 65536);
  assert(d.x == b.x);
  assert(d.y == b.y);

  point_t e = curve_point_on_line(a, b, 65536 / 2);
  assert(e.x == 50);
  assert(e.y == 50);

  point_t f = curve_point_on_line(a, b, 65536 * 1 / 3);
  assert(f.x == 33);
  assert(f.y == 33);

  point_t g = curve_point_on_line(a, b, 65536 * 2 / 3);
  assert(g.x == 66);
  assert(g.y == 66);

  return result_TEST_PASSED;
}

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

static void setup_all_curves(curveteststate_t *state)
{
  int cpi;
  int y;
  int npoints;
  int set;
  int width;
  int lefthand;
  int i;

  cpi = 0; /* control point index */
  y   = BORDER;
  for (set = 0; set < NSETS; set++)
  {
    npoints  = curves[set].npoints;
    width    = (npoints - 1) * UNIT;
    lefthand = (state->scr_width - width) / 2;

    for (i = 0; i < npoints; i++)
    {
      state->control_points[cpi].x = lefthand + i * UNIT;
      state->control_points[cpi].y = (i > 0 && i < npoints - 1) ? y : y + UNIT;
      cpi++;
    }

    y += state->section_height;
  }
}

static void draw_all_control_points(curveteststate_t *state)
{
  int      cpi;
  int      npoints;
  int      set;
  int      i;
  box_t    b;
  colour_t colour;

  cpi = 0;
  for (set = 0; set < NSETS; set++)
  {
    npoints = curves[set].npoints;

    for (i = 0; i < npoints; i++)
    {
      b.x0 = state->control_points[cpi].x - BLOBSZ / 2;
      b.y0 = state->control_points[cpi].y - BLOBSZ / 2;
      b.x1 = state->control_points[cpi].x + BLOBSZ / 2;
      b.y1 = state->control_points[cpi].y + BLOBSZ / 2;

      colour = state->palette[8 + set];

      screen_draw_square(&state->scr, b.x0, b.y0, BLOBSZ, colour);

      box_union(&b, &state->overalldirty, &state->overalldirty);

      cpi++;
    }
  }
}

/* draw curve from draw_points[] */
static void draw_a_curve(curveteststate_t *state)
{
  int      i;
  box_t    b;
  colour_t colour;

  for (i = 0; i < state->nsegments; i++)
  {
    b.x0 = state->draw_points[i + 0].x;
    b.y0 = state->draw_points[i + 0].y;
    b.x1 = state->draw_points[i + 1].x;
    b.y1 = state->draw_points[i + 1].y;

    if (state->opt.checker)
      colour = state->palette[(i & 1) ? palette_PICO8_DARK_PURPLE : palette_PICO8_LIGHT_PEACH];
    else
      colour = state->palette[palette_PICO8_BLACK];

    if (state->opt.draw_endpoints)
    {
      screen_draw_pixel(&state->scr, b.x0, b.y0, state->palette[palette_PICO8_GREEN]);
      screen_draw_pixel(&state->scr, b.x1, b.y1, state->palette[palette_PICO8_RED]);
    }

    if (state->opt.use_aa)
      screen_draw_line_wu_float(&state->scr, b.x0, b.y0, b.x1, b.y1, colour);
    else
      screen_draw_line(&state->scr, b.x0, b.y0, b.x1, b.y1, colour);

    box_union(&b, &state->overalldirty, &state->overalldirty);
  }
}

/* calculate curves */
static void calc_all_curves(curveteststate_t *state)
{
  int set;
  int o;
  int np;
  int i;
  int t;

  for (set = 0; set < NELEMS(curves); set++)
  {
    o = curves[set].offset;
    np = curves[set].npoints;

    if (curves[set].kind == FwdDiffCubic)
    {
      curve_bezier_cubic_f(state->jitterfn(state->control_points[o + 0]),
                         state->jitterfn(state->control_points[o + 1]),
                         state->jitterfn(state->control_points[o + 2]),
                         state->jitterfn(state->control_points[o + 3]),
                         state->nsegments,
                        &state->draw_points[0]);
    }
    else
    {
      state->draw_points[0]                = state->jitterfn(state->control_points[o]);
      state->draw_points[state->nsegments] = state->jitterfn(state->control_points[o + np - 1]);

      for (i = 1; i < state->nsegments; i++)
      {
        t = 65536 * i / state->nsegments;

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
    }

    draw_a_curve(state);
  }
}

/* rotate a line or three */
static void rotate_lines(curveteststate_t *state, int degrees, int palidx)
{
  static float spin       = 0.0f;

  const float  radius     = 5.0f;
  const float  diameter   = radius * 2.0f;
  const float  deg2rad    = M_PI * 2.0f / 360.0f;
  const int    nvert      = (state->scr_height + diameter / 2.0f) / diameter;
  const int    nhorz      = 7;
  const float  separation = diameter;
  const int    ntypes     = 3 + 1;

  float       startx, starty;
  int         x,y;
  float       r;
  float       xa, ya, xb, yb;
  float       sep;
  box_t       b;

  startx = starty = radius;

  for (y = 0; y < nvert; y++)
  {
    for (x = 0; x < nhorz; x++)
    {
      r = spin;
      r += degrees / 8.0f; /* spin with input */
      r +=  90.0f * x / (nhorz - 1);
      r += 360.0f * y / (nvert - 1); /* show all rotations over the range */

      spin += 0.0005f; /* spin very slowly over time */

      xa = startx + x * diameter * ntypes + sinf((r +   0.0f) * deg2rad) * radius;
      ya = starty + y * diameter * ntypes + cosf((r +   0.0f) * deg2rad) * radius;
      xb = startx + x * diameter * ntypes + sinf((r + 180.0f) * deg2rad) * radius;
      yb = starty + y * diameter * ntypes + cosf((r + 180.0f) * deg2rad) * radius;

      sep = 0;

      screen_draw_line(&state->scr,
                        xa + sep, ya, xb + sep, yb,
                        state->palette[palidx]);
      sep += separation;

      screen_draw_line_wu_float(&state->scr,
                                 xa + sep, ya, xb + sep, yb,
                                 state->palette[palidx]);
      sep += separation;

      screen_draw_line_wu_fix8(&state->scr,
                                FLOAT_TO_FIX8(xa + sep),
                                FLOAT_TO_FIX8(ya),
                                FLOAT_TO_FIX8(xb + sep),
                                FLOAT_TO_FIX8(yb),
                                state->palette[palidx]);

      box_reset(&b);
      box_extend_n(&b, (int) xa, (int) ya, (int) xb, (int) yb);
      box_union(&b, &state->overalldirty, &state->overalldirty);
    }
  }
}

static result_t curve_interactive_test(curveteststate_t *state)
{
  bool  quit          = false;
  int   frame;
  int   mx            = 0;
  int   my            = 0;
  int   firstdraw     = 1;
  bool  opt_dontclear = false;
  box_t prevdirty     = BOX_INIT;
  int   dragging      = -1;
  int   i;

  state->opt.use_aa         = true;
  state->opt.checker        = true;
  state->opt.draw_endpoints = true;

  state->nsegments          = 32;
  box_reset(&state->overalldirty);
  state->jitterfn           = &jitter_off;

  state->section_height     = (state->scr_height - 2 * BORDER) / NSETS; /* divide screen into chunks */

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
          case SDLK_a:
            state->opt.use_aa = !state->opt.use_aa;
            break;
          case SDLK_c:
            state->opt.checker = !state->opt.checker;
            break;
          case SDLK_j:
            state->jitterfn = (state->jitterfn == jitter_on) ? &jitter_off : &jitter_on;
            break;
          case SDLK_p:
            state->opt.draw_endpoints = !state->opt.draw_endpoints;
            break;
          case SDLK_q:
            quit = true; break;
          }
          break;

        case SDL_MOUSEMOTION:
          mx = event.motion.x;
          my = event.motion.y;

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
            state->nsegments = MIN(state->nsegments + 1, MAXSEGMENTS);
          else if (event.motion.yrel < 0)
            state->nsegments = MAX(state->nsegments - 1, MINSEGMENTS);
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
      bitmap_clear(&state->bm, state->palette[state->background_palette_index]);
      box_reset(&state->overalldirty);
    }

    draw_all_control_points(state);
    calc_all_curves(state);
    rotate_lines(state, mx, state->opt.checker ? palette_PICO8_DARK_GREEN : palette_PICO8_BLACK);

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

result_t curve_test_one_format(const char *resources,
                               int         scr_width,
                               int         scr_height,
                               pixelfmt_t  scr_fmt)
{
  result_t          rc = result_OK;
  curveteststate_t  state;
  unsigned int     *pixels;
  int               bm_inited = 0;

  NOT_USED(resources);

  state.scr_width  = scr_width;
  state.scr_height = scr_height;

  const int scr_rowbytes = (scr_width << pixelfmt_log2bpp(scr_fmt)) / 8;

  define_pico8_palette(&state.palette[0]);

  state.transparent = colour_rgba(0x00, 0x00, 0x00, 0x00);

  state.background_palette_index = 7; /* near white */

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

  bitmap_clear(&state.bm, state.palette[state.background_palette_index]);

  screen_for_bitmap(&state.scr, &state.bm);

  /* ------------------------------------------------------------------------ */

  typedef result_t (*curvetestfn_t)(curveteststate_t *);

  static const curvetestfn_t tests[] =
  {
    curve_basic_test,
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
    { 800, 600, pixelfmt_bgrx8888 }
  };

  result_t rc;
  int      i;

  for (i = 0; i < NELEMS(tab); i++)
  {
    rc = curve_test_one_format(resources,
                               tab[i].width, tab[i].height,
                               tab[i].fmt);
    if (rc != result_TEST_PASSED)
      return rc;
  }

  return rc;
}

/* vim: set ts=8 sts=2 sw=2 et: */
