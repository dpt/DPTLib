/* wuss/script.c -- scripted input and state dumps for the Wuss demo */

#ifdef WUSS_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"
#include "wuss/window.h"
#include "wuss/wuss.h"

/* white-box: dump walks the z-order lists directly */
#include "../../libraries/wuss/core/impl.h"

#include "frontend.h"
#include "script.h"

/* ----------------------------------------------------------------------- */

static struct
{
  FILE         *f;
  const char   *name;
  int           lineno;
  char          line[512];
  bool          held;      /* line holds a dump/png deferred to next frame */
  wuss_input_t  queue[3];  /* a click's move, down and up */
  int           qhead, qtail;
  char          text[256]; /* a type command's characters */
  const char   *text_pos;
  int           wait;      /* frames left to let pass */
  double        until;     /* sleep's wall-clock deadline, or 0 */
  bool          emitted;   /* input sent since the last frame boundary */
  bool          failed;
}
g;

/* ----------------------------------------------------------------------- */

static void script_error(const char *msg, const char *arg)
{
  fprintf(stderr, "%s:%d: %s%s%s\n", g.name, g.lineno, msg,
          arg ? ": " : "", arg ? arg : "");
  g.failed = true;
}

/* Splits off the next space-separated word of *p, or a "quoted" run.
 * Returns NULL at end of line or at a '#' comment. */
static char *next_token(char **p)
{
  char *s;
  char *start;

  s = *p;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
    s++;
  if (*s == '\0' || *s == '#')
    return NULL;

  if (*s == '"')
  {
    start = ++s;
    while (*s != '\0' && *s != '"')
      s++;
  }
  else
  {
    start = s;
    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
      s++;
  }
  if (*s != '\0')
    *s++ = '\0';

  *p = s;
  return start;
}

/* Front-most window titled title, or NULL. */
static wuss_window_t *find_window(wuss_t *wuss, const char *title)
{
  int     stack;
  list_t *link;

  for (stack = 0; stack < WUSS_STACK_COUNT; stack++)
    for (link = wuss->z_order[stack].next; link != NULL; link = link->next)
      if (strcmp(wuss_window_get_title((wuss_window_t *) link), title) == 0)
        return (wuss_window_t *) link;

  return NULL;
}

/* Parses "[TITLE] X Y" from *p into a screen point. A first token that
 * isn't a number is taken as a window title, X Y then being relative to
 * that window's content top-left. */
static bool parse_point(wuss_t *wuss, char **p, point_t *out)
{
  char          *tok;
  char          *end;
  wuss_window_t *window;
  box_t          content;
  long           x, y;

  out->x = 0;
  out->y = 0;

  tok = next_token(p);
  if (tok == NULL)
  {
    script_error("expected a point", NULL);
    return false;
  }

  x = strtol(tok, &end, 10);
  if (end == tok || *end != '\0')
  {
    window = find_window(wuss, tok);
    if (window == NULL)
    {
      script_error("no window titled", tok);
      return false;
    }
    wuss_window_get_content_bounds(window, &content);
    out->x = content.x0;
    out->y = content.y0;

    tok = next_token(p);
    x   = tok ? strtol(tok, &end, 10) : 0;
    if (tok == NULL || end == tok || *end != '\0')
    {
      script_error("expected X", tok);
      return false;
    }
  }

  tok = next_token(p);
  y   = tok ? strtol(tok, &end, 10) : 0;
  if (tok == NULL || end == tok || *end != '\0')
  {
    script_error("expected Y", tok);
    return false;
  }

  out->x += (int) x;
  out->y += (int) y;
  return true;
}

/* Strips leading "MOD+" prefixes from *tok into *mods. A trailing '+' is
 * left as the rest of the token, as in "ctrl++". */
static bool parse_mods(char **tok, wuss_key_modifiers_t *mods)
{
  char *plus;

  *mods = wuss_KEY_MOD_NONE;
  while ((plus = strchr(*tok, '+')) != NULL && plus[1] != '\0')
  {
    *plus = '\0';
    if (strcmp(*tok, "shift") == 0)
      *mods |= wuss_KEY_MOD_SHIFT;
    else if (strcmp(*tok, "ctrl") == 0)
      *mods |= wuss_KEY_MOD_CTRL;
    else if (strcmp(*tok, "alt") == 0)
      *mods |= wuss_KEY_MOD_ALT;
    else
      return false;
    *tok = plus + 1;
  }

  return true;
}

/* Parses "[MOD+...]BUTTON" into a button and modifiers. */
static bool parse_button(char                 *tok,
                         wuss_button_t        *out,
                         wuss_key_modifiers_t *mods)
{
  if (tok == NULL || !parse_mods(&tok, mods))
    return false;
  else if (strcmp(tok, "select") == 0)
    *out = wuss_BUTTON_SELECT;
  else if (strcmp(tok, "menu") == 0)
    *out = wuss_BUTTON_MENU;
  else if (strcmp(tok, "adjust") == 0)
    *out = wuss_BUTTON_ADJUST;
  else
    return false;

  return true;
}

/* Parses "[MOD+...]KEY" into a key code and modifiers. */
static bool parse_key(char *tok, int *key, wuss_key_modifiers_t *mods)
{
  static const struct
  {
    const char *name;
    int         key;
  }
  names[] =
  {
    { "return",    wuss_KEY_RETURN    },
    { "escape",    wuss_KEY_ESCAPE    },
    { "tab",       wuss_KEY_TAB       },
    { "backspace", wuss_KEY_BACKSPACE },
    { "space",     ' '                },
    { "delete",    wuss_KEY_DELETE    },
    { "insert",    wuss_KEY_INSERT    },
    { "up",        wuss_KEY_UP        },
    { "down",      wuss_KEY_DOWN      },
    { "left",      wuss_KEY_LEFT      },
    { "right",     wuss_KEY_RIGHT     },
    { "home",      wuss_KEY_HOME      },
    { "end",       wuss_KEY_END       },
    { "pageup",    wuss_KEY_PAGE_UP   },
    { "pagedown",  wuss_KEY_PAGE_DOWN }
  };

  int i;
  int n;

  *mods = wuss_KEY_MOD_NONE;
  if (tok == NULL || !parse_mods(&tok, mods))
    return false;

  if (tok[0] != '\0' && tok[1] == '\0')
  {
    *key = (unsigned char) tok[0];
    return true;
  }

  for (i = 0; i < (int) NELEMS(names); i++)
  {
    if (strcmp(tok, names[i].name) == 0)
    {
      *key = names[i].key;
      return true;
    }
  }

  if ((tok[0] == 'f' || tok[0] == 'F') && (n = atoi(tok + 1)) >= 1 && n <= 12)
  {
    *key = wuss_KEY_F1 + n - 1;
    return true;
  }

  return false;
}

/* ----------------------------------------------------------------------- */

static void dump(wuss_t *wuss)
{
  static const char *stack_names[WUSS_STACK_COUNT] = { "top", "middle",
                                                       "back" };

  wuss_window_t *focus;
  int            stack;
  list_t        *link;
  wuss_window_t *window;
  box_t          content;

  focus = wuss_get_focus(wuss);

  printf("dump %s:%d\n", g.name, g.lineno);
  for (stack = 0; stack < WUSS_STACK_COUNT; stack++)
  {
    for (link = wuss->z_order[stack].next; link != NULL; link = link->next)
    {
      window = (wuss_window_t *) link;
      wuss_window_get_content_bounds(window, &content);
      printf("  window \"%s\" %s visible=%d,%d,%d,%d content=%d,%d,%d,%d%s%s"
             "\n",
             wuss_window_get_title(window), stack_names[stack],
             window->visible.x0, window->visible.y0,
             window->visible.x1, window->visible.y1,
             content.x0, content.y0, content.x1, content.y1,
             (window == focus) ? " focus" : "",
             (window->flags & wuss_WINDOW_HIDDEN) ? " hidden" : "");
    }
  }
  fflush(stdout);
}

/* PNG save handles paletted and bgrx8888; take anything else via bgrx8888. */
result_t script_save_png(const bitmap_t *bm, const char *filename)
{
  result_t  rc;
  bitmap_t *deep;

  if (pixelfmt_paletted_nentries(bm->format) > 0 ||
      bm->format == pixelfmt_bgrx8888)
  {
    rc = bitmap_save_png(bm, filename);
  }
  else
  {
    rc = bitmap_convert(bm, pixelfmt_bgrx8888, &deep);
    if (rc == result_OK)
    {
      rc = bitmap_save_png(deep, filename);
      free(deep->base);
      free(deep->palette);
      free(deep);
    }
  }

  return rc;
}

/* ----------------------------------------------------------------------- */

result_t script_open(const char *filename)
{
  memset(&g, 0, sizeof(g));

  g.f = fopen(filename, "r");
  if (g.f == NULL)
  {
    fprintf(stderr, "wuss: cannot open script \"%s\"\n", filename);
    return result_NOT_FOUND;
  }

  g.name = filename;
  return result_OK;
}

result_t script_close(void)
{
  if (g.f != NULL)
    fclose(g.f);
  g.f = NULL;

  return g.failed ? result_BAD_ARG : result_OK;
}

static double script_now(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Ends the script's part of the current frame. */
static bool end_frame(void)
{
  g.emitted = false;
  return false;
}

static void queue_input(wuss_input_kind_t    kind,
                        point_t              pos,
                        wuss_button_t        button,
                        wuss_key_modifiers_t mods)
{
  wuss_input_t *ev;

  ev = &g.queue[g.qtail++];
  memset(ev, 0, sizeof(*ev));
  ev->kind   = kind;
  ev->pos    = pos;
  ev->button = button;
  ev->mods   = mods;
}

bool script_poll(wuss_t *wuss, const bitmap_t *bm, wuss_input_t *ev)
{
  char                *p;
  char                *cmd;
  char                *arg;
  wuss_button_t        button;
  point_t              pos;
  int                  key;
  wuss_key_modifiers_t mods;

  for (;;)
  {
    if (g.qhead < g.qtail)
    {
      *ev = g.queue[g.qhead++];
      g.emitted = true;
      return true;
    }
    g.qhead = g.qtail = 0;

    if (g.text_pos != NULL && *g.text_pos != '\0')
    {
      memset(ev, 0, sizeof(*ev));
      ev->kind = wuss_INPUT_KEY;
      ev->key  = (unsigned char) *g.text_pos++;
      g.emitted = true;
      return true;
    }

    if (g.wait > 0)
    {
      g.wait--;
      return end_frame();
    }

    if (g.until > 0.0)
    {
      if (script_now() < g.until)
        return end_frame();

      g.until = 0.0;
    }

    if (g.f == NULL)
      return end_frame();

    if (!g.held)
    {
      if (g.failed ||
          fgets(g.line, sizeof(g.line), g.f) == NULL)
      {
        fclose(g.f);
        g.f = NULL;
        memset(ev, 0, sizeof(*ev));
        ev->kind = wuss_INPUT_QUIT;
        return true;
      }
      g.lineno++;
    }

    p   = g.line;
    cmd = next_token(&p);
    if (cmd == NULL)
      continue;

    if (strcmp(cmd, "dump") == 0 || strcmp(cmd, "png") == 0)
    {
      /* let the input already sent this frame be redrawn first */
      if (g.emitted)
      {
        /* next_token cut the line at cmd; put the separator back */
        if (p > cmd + strlen(cmd))
          cmd[strlen(cmd)] = ' ';
        g.held = true;
        return end_frame();
      }
      g.held = false;

      if (cmd[0] == 'd')
      {
        dump(wuss);
      }
      else
      {
        arg = next_token(&p);
        if (arg == NULL)
          script_error("png needs a filename", NULL);
        else
          if (script_save_png(bm, arg) != result_OK)
            script_error("png save failed", arg);
      }
    }
    else if (strcmp(cmd, "move") == 0)
    {
      if (parse_point(wuss, &p, &pos))
        queue_input(wuss_INPUT_MOUSE_MOVE, pos, wuss_BUTTON_NONE,
                    wuss_KEY_MOD_NONE);
    }
    else if (strcmp(cmd, "down") == 0 || strcmp(cmd, "up") == 0 ||
             strcmp(cmd, "click") == 0)
    {
      if (!parse_button(next_token(&p), &button, &mods))
      {
        script_error("expected select, menu or adjust", NULL);
      }
      else if (parse_point(wuss, &p, &pos))
      {
        queue_input(wuss_INPUT_MOUSE_MOVE, pos, wuss_BUTTON_NONE,
                    wuss_KEY_MOD_NONE);
        if (cmd[0] != 'u')
          queue_input(wuss_INPUT_MOUSE_DOWN, pos, button, mods);
        if (cmd[0] != 'd')
          queue_input(wuss_INPUT_MOUSE_UP, pos, button, mods);
      }
    }
    else if (strcmp(cmd, "key") == 0 || strcmp(cmd, "keyup") == 0)
    {
      arg = next_token(&p);
      if (!parse_key(arg, &key, &mods))
      {
        script_error("unknown key", arg);
      }
      else
      {
        memset(ev, 0, sizeof(*ev));
        ev->kind = strcmp(cmd, "key") == 0 ? wuss_INPUT_KEY
                                             : wuss_INPUT_KEY_UP;
        ev->key  = key;
        ev->mods = mods;
        g.emitted = true;
        return true;
      }
    }
    else if (strcmp(cmd, "type") == 0)
    {
      arg = next_token(&p);
      if (arg == NULL)
      {
        script_error("type needs text", NULL);
      }
      else
      {
        snprintf(g.text, sizeof(g.text), "%s", arg);
        g.text_pos = g.text;
      }
    }
    else if (strcmp(cmd, "wait") == 0)
    {
      arg = next_token(&p);
      g.wait = arg ? atoi(arg) : 1;
    }
    else if (strcmp(cmd, "sleep") == 0)
    {
      arg = next_token(&p);
      g.until = script_now() + (arg ? atof(arg) : 1.0);
    }
    else if (strcmp(cmd, "quit") == 0)
    {
      fclose(g.f);
      g.f = NULL;
      memset(ev, 0, sizeof(*ev));
      ev->kind = wuss_INPUT_QUIT;
      return true;
    }
    else
    {
      script_error("unknown command", cmd);
    }
  }
}

#endif /* WUSS_APP */
