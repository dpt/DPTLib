/* wuss/frontend-riscos.c -- native RISC OS backend for the Wuss demo */

#ifdef WUSS_APP
#ifdef __riscos

#include <stdio.h>
#include <stdlib.h>

#include "kernel.h"

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "swis.h"

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/point.h"
#include "wuss/wuss.h"

#include "frontend.h"

/* This backend takes over the whole screen: it selects a 16-colour linear
 * mode, points wuss's framebuffer straight at screen memory (so a redraw
 * lands on the display with no blit) and polls OS_Mouse / the keyboard each
 * frame. It restores the entry mode on close. There is no Wimp task here --
 * wuss's own window furniture is the entire UI. */

/* OS_ReadVduVariables indices we ask for, in this order. */
enum
{
  VDUVAR_XEIG      = 4,   /* OS-units-per-pixel shift, x */
  VDUVAR_YEIG      = 5,   /* OS-units-per-pixel shift, y */
  VDUVAR_LINE_LEN  = 6,   /* bytes per screen row */
  VDUVAR_YWIND_LIM = 11,  /* max Y in OS units (screen height - 1, << YEIG) */
  VDUVAR_DISPLAY_START = 149 /* base address of screen memory */
};

/* A 16-colour mode 640 x 480. Mode 27 is 640x480x16 on RISC OS; if a given
 * machine lacks it, OS_ScreenMode substitutes the closest match and the code
 * below reads back whatever it actually got. */
#define WUSS_RISCOS_MODE 27

struct wuss_frontend
{
  int   entry_mode;   /* mode number to restore on close */
  int   scr_width;
  int   scr_height;
  void *screen_base;  /* screen memory: wuss draws here directly */
  int   rowbytes;
  int   xeig, yeig;   /* OS_Mouse returns OS units; >> eig gives pixels */
  int   last_buttons; /* OS_Mouse button state at the previous poll */
};

/* ----------------------------------------------------------------------- */

/* Program the hardware palette from wuss's colour_t[]. colour_t.primary is
 * pixelfmt_rgba8888: R in the low byte, then G, then B -- the same byte order
 * OS_Word 12's palette block wants. */
static void set_hw_palette(const colour_t *palette, int npalette)
{
  int i;
  int n = (npalette < 16) ? npalette : 16;

  for (i = 0; i < n; i++)
  {
    unsigned int  rgba = palette[i].primary;
    unsigned char block[5];

    /* block = logical colour, 16 (set both flash states), R, G, B */
    block[0] = (unsigned char) i;
    block[1] = 16;
    block[2] = (unsigned char) (rgba & 0xFF);
    block[3] = (unsigned char) ((rgba >> 8) & 0xFF);
    block[4] = (unsigned char) ((rgba >> 16) & 0xFF);

    _swix(OS_Word, _INR(0,1), 12, block);
  }
}

/* ----------------------------------------------------------------------- */

result_t wuss_frontend_open(int               width,
                            int               height,
                            const colour_t   *palette,
                            int               npalette,
                            void            **pixels,
                            int              *rowbytes,
                            pixelfmt_t       *fmt,
                            wuss_frontend_t **frontend)
{
  wuss_frontend_t *fe;
  int              vars[6]; /* 5 indices + a -1 terminator */
  int              vals[5];
  _kernel_oserror *err;
  char             msg[160]; /* deferred so it prints after the mode restore */

  msg[0] = '\0';

  fe = calloc(1, sizeof(*fe));
  if (fe == NULL)
    return result_OOM;

  /* remember the mode we came in on */
  err = _swix(OS_ScreenMode, _IN(0) | _OUT(1), 1, &fe->entry_mode);
  if (err != NULL)
  {
    snprintf(msg, sizeof(msg), "OS_ScreenMode read: %s", err->errmess);
    goto failure;
  }

  /* switch to the 16-colour demo mode */
  err = _swix(OS_ScreenMode, _INR(0,1), 0, WUSS_RISCOS_MODE);
  if (err != NULL)
  {
    snprintf(msg, sizeof(msg), "OS_ScreenMode set mode %d: %s",
             WUSS_RISCOS_MODE, err->errmess);
    goto failure;
  }

  /* read back the geometry we actually got */
  vars[0] = VDUVAR_XEIG;
  vars[1] = VDUVAR_YEIG;
  vars[2] = VDUVAR_LINE_LEN;
  vars[3] = VDUVAR_YWIND_LIM;
  vars[4] = VDUVAR_DISPLAY_START;
  vars[5] = -1; /* OS_ReadVduVariables scans until it hits -1 */
  err = _swix(OS_ReadVduVariables, _INR(0,1), vars, vals);
  if (err != NULL)
  {
    snprintf(msg, sizeof(msg), "OS_ReadVduVariables: %s", err->errmess);
    goto failure_restore;
  }

  fe->xeig        = vals[0];
  fe->yeig        = vals[1];
  fe->rowbytes    = vals[2];
  /* VDU var 11 (YWindLimit) is the top pixel row, in pixels not OS units */
  fe->scr_height  = vals[3] + 1;
  fe->screen_base = (void *) vals[4];
  fe->scr_width   = fe->rowbytes * 2; /* 4bpp: 2 pixels per byte */
  fe->last_buttons = 0;

  /* the demo asks for a fixed size; if the mode came out smaller the tasks
   * would draw off-screen, so bail rather than corrupt memory */
  if (fe->scr_width < width || fe->scr_height < height)
  {
    snprintf(msg, sizeof(msg),
             "mode %d gave %dx%d (xeig=%d yeig=%d linelen=%d ywindlim=%d),"
             " need %dx%d",
             WUSS_RISCOS_MODE, fe->scr_width, fe->scr_height,
             vals[0], vals[1], vals[2], vals[3], width, height);
    goto failure_restore;
  }

  /* the mode may be bigger than the demo; hand wuss only the area it asked
   * for so a redraw can never run off the end of screen memory */
  fe->scr_width  = width;
  fe->scr_height = height;

  set_hw_palette(palette, npalette);

  /* hide the text cursor: VDU 23,1,0 -- wuss draws over the whole screen and
   * a blinking caret in the corner would show through */
  _swix(OS_WriteN, _INR(0,1), "\x17\x01\x00\x00\x00\x00\x00\x00\x00\x00", 10);

  /* OS_ScreenMode leaves the pointer off. Turn on pointer 1 with the default
   * arrow shape (*Pointer 1 == OS_Byte 106, 1) and confine it to the screen.
   * wuss draws no cursor of its own, so this is the pointer the user sees. */
  _swix(OS_Byte, _INR(0,1), 106, 1);

  *pixels   = fe->screen_base;
  *rowbytes = fe->rowbytes;
  *fmt      = pixelfmt_p4; /* RISC OS 4bpp: leftmost pixel in the low nibble */
  *frontend = fe;
  return result_OK;


failure_restore:

  _swix(OS_ScreenMode, _INR(0,1), 0, fe->entry_mode);

failure:

  /* mode is restored to text by now, so this reaches the screen */
  if (msg[0] != '\0')
    fprintf(stderr, "wuss: frontend_open: %s\n", msg);
  free(fe);
  return result_TEST_FAILED;
}

/* Map an OS_Mouse button word (bit 2 = Select/left, bit 1 = Menu/middle,
 * bit 0 = Adjust/right on a 3-button mouse) to wuss_button_t, whose bit
 * values already match RISC OS order. */
static wuss_button_t mouse_buttons_to_wuss(int buttons)
{
  wuss_button_t b = wuss_BUTTON_NONE;

  if (buttons & 4) b |= wuss_BUTTON_SELECT;
  if (buttons & 2) b |= wuss_BUTTON_MENU;
  if (buttons & 1) b |= wuss_BUTTON_ADJUST;
  return b;
}

/* Keys the demo reacts to: negative INKEY scan code -> input kind. Q and
 * Escape both quit. */
static const struct
{
  int               scan;
  wuss_input_kind_t kind;
}
g_keys[] =
{
  { -17,  wuss_INPUT_QUIT          }, /* Q */
  { -113, wuss_INPUT_QUIT          }, /* Escape */
  { -114, wuss_INPUT_REDRAW_ALL    }, /* F1 */
  { -116, wuss_INPUT_PIXEL_STRESS  }, /* F3 */
  { -117, wuss_INPUT_PALETTE_CYCLE }  /* F4 */
};

static bool key_down(int scan)
{
  int r1, r2;

  /* OS_Byte 129 with R1=scan, R2=0xFF: R1 = 0xFF if that key is pressed. */
  _swix(OS_Byte, _INR(0,2) | _OUTR(1,2), 129, scan & 0xFF, 0xFF, &r1, &r2);
  return (r1 & 0xFF) == 0xFF;
}

bool wuss_frontend_poll(wuss_frontend_t *fe, wuss_input_t *event)
{
  static unsigned int key_was; /* bit i = g_keys[i] was down last poll */

  unsigned int key_now = 0;
  int          mx, my, buttons, t;
  int          px, py;
  size_t       i;

  /* keyboard first: fire one event on the press edge so a held key doesn't
   * repeat every frame */
  for (i = 0; i < NELEMS(g_keys); i++)
    if (key_down(g_keys[i].scan))
      key_now |= 1u << i;

  for (i = 0; i < NELEMS(g_keys); i++)
  {
    unsigned int bit = 1u << i;

    if ((key_now & bit) && !(key_was & bit))
    {
      key_was = key_now;
      event->kind = g_keys[i].kind;
      return true;
    }
  }
  key_was = key_now;

  /* poll the mouse: OS_Mouse returns x,y in OS units and a button word */
  if (_swix(OS_Mouse, _OUTR(0,3), &mx, &my, &buttons, &t) != NULL)
    return false;

  /* OS units -> pixels; y is bottom-up on RISC OS, wuss wants top-down */
  px = mx >> fe->xeig;
  py = (fe->scr_height - 1) - (my >> fe->yeig);

  if (buttons != fe->last_buttons)
  {
    int pressed  = buttons & ~fe->last_buttons;
    int released = fe->last_buttons & ~buttons;

    fe->last_buttons = buttons;

    if (pressed)
    {
      event->kind   = wuss_INPUT_MOUSE_DOWN;
      event->pos    = POINT(px, py);
      event->button = mouse_buttons_to_wuss(pressed);
      return true;
    }
    if (released)
    {
      event->kind   = wuss_INPUT_MOUSE_UP;
      event->pos    = POINT(px, py);
      event->button = mouse_buttons_to_wuss(released);
      return true;
    }
  }

  /* no button change: report the move once, then say the queue is empty */
  {
    static int last_px = -1, last_py = -1;

    if (px != last_px || py != last_py)
    {
      last_px = px;
      last_py = py;
      event->kind = wuss_INPUT_MOUSE_MOVE;
      event->pos  = POINT(px, py);
      return true;
    }
  }

  return false;
}

void wuss_frontend_present(wuss_frontend_t *fe, const bitmap_t *bm)
{
  NOT_USED(fe);
  NOT_USED(bm);

  /* wuss drew straight into screen memory; just pace to the frame rate */
  _swix(OS_Byte, _IN(0), 19);
}

void wuss_frontend_set_palette(wuss_frontend_t *fe,
                               const colour_t  *palette,
                               int              npalette)
{
  NOT_USED(fe);
  set_hw_palette(palette, npalette);
}

void wuss_frontend_close(wuss_frontend_t *fe)
{
  if (fe == NULL)
    return;

  _swix(OS_Byte, _INR(0,1), 106, 0); /* *Pointer 0: turn the pointer off */
  _swix(OS_WriteN, _INR(0,1),        /* VDU 23,1,1: text cursor back on */
        "\x17\x01\x01\x00\x00\x00\x00\x00\x00\x00", 10);
  _swix(OS_ScreenMode, _INR(0,1), 0, fe->entry_mode);
  free(fe);
}

#endif /* __riscos */
#endif /* WUSS_APP */
