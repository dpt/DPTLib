/* wuss/helpers/create-from-desc.c -- build a menu tree from a descriptor string */

/* The token machine (Token / Parser / getopt / getname / getnext / isdelim)
 * and the explicit menu stack are ported from PrivateEye's
 * libs/appengine/wimp/menu/create-from-desc.c. What changed for wuss: the
 * output is a heap wuss_menu_t tree rather than a wimp_menu, there is no title
 * row (the first token of a { } block is parsed then discarded, so descriptor
 * strings stay source-compatible), and a '>' submenu is deep-copied from the
 * const wuss_menu_t * vararg so the whole result tree is owned uniformly and
 * freed by one wuss_menu_destroy. */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "utils/array.h"

#include "wuss/menu-desc.h"

/* ----------------------------------------------------------------------- */

enum { WUSS_MENU_DESC_DEPTH = 8 };   /* nested { } limit, as the Wimp original */
enum { WUSS_MENU_DESC_TEXT  = 64 };  /* per-token expansion buffer */

/* ----------------------------------------------------------------------- */

typedef enum { Name, Sep, DashSep, Push, Pop, End, Error } Token;

enum { Tick = 1 << 0, Shade = 1 << 1, SubMenu = 1 << 2 };

typedef struct
{
  const char  *p;
  const char  *start, *end; /* end exclusive */
  unsigned int flags;
}
Parser;

static void getopt_(Parser *parser)
{
  const char *p;
  int         c;

  parser->flags = 0;
  p = parser->p;

  for (;;)
  {
    c = *p;
    if      (c == '!') parser->flags |= Tick;
    else if (c == '>') parser->flags |= SubMenu;
    else if (c == '~') parser->flags |= Shade;
    else { parser->p = p; return; }
    p++;
  }
}

static int isdelim(int c)
{
  return c == ',' || c == '|' || c == '{' || c == '}' || c == '\0';
}

static void getname(Parser *parser)
{
  const char *p;

  getopt_(parser);
  p = parser->p;
  parser->start = p;

  while (!isdelim(*p))
    p++;

  parser->p = p;

  /* trim trailing whitespace so "Foo { " yields the name "Foo", not "Foo " */
  while (p > parser->start && isspace((unsigned char) p[-1]))
    p--;

  parser->end = p;
}

static Token getnext(Parser *parser)
{
  while (isspace((unsigned char) *parser->p))
    parser->p++;

  if (!isdelim(*parser->p))
  {
    getname(parser);
    if (parser->start == parser->end)
      return Error;
    return Name;
  }

  switch (*parser->p++)
  {
  case ',': return Sep;
  case '|': return DashSep;
  case '{': return Push;
  case '}': return Pop;
  }

  assert(parser->p[-1] == '\0');
  return End;
}

/* ----------------------------------------------------------------------- */

void wuss_menu_destroy(wuss_menu_t *menu)
{
  int i;

  if (menu == NULL)
    return;

  for (i = 0; i < menu->nitems; i++)
  {
    free((void *) menu->items[i].text); /* discard const */
    wuss_menu_destroy((wuss_menu_t *) menu->items[i].submenu);
  }
  free((void *) menu->title); /* discard const */
  free((void *) menu->items); /* discard const */
  free(menu);
}

/* Deep-copy a caller-supplied menu tree so a '>' submenu can be owned by, and
 * freed with, the result of wuss_menu_create_from_desc. */
static result_t menu_deep_copy(const wuss_menu_t *src, wuss_menu_t **out)
{
  wuss_menu_t      *m;
  wuss_menu_item_t *items;
  int               i;
  result_t          rc;

  m = malloc(sizeof(*m));
  if (m == NULL)
    return result_OOM;

  items = (src->nitems > 0)
        ? calloc((size_t) src->nitems, sizeof(*items))
        : NULL;
  if (src->nitems > 0 && items == NULL)
  {
    free(m);
    return result_OOM;
  }

  m->title  = src->title ? strdup(src->title) : NULL;
  if (src->title != NULL && m->title == NULL)
  {
    free(items);
    free(m);
    return result_OOM;
  }

  m->items  = items;
  m->nitems = src->nitems;

  for (i = 0; i < src->nitems; i++)
  {
    items[i].text  = strdup(src->items[i].text ? src->items[i].text : "");
    items[i].flags = src->items[i].flags;
    items[i].submenu = NULL;
    items[i].window  = src->items[i].window; /* borrowed, as in the source */
    items[i].swatch  = src->items[i].swatch;

    if (items[i].text == NULL)
    {
      m->nitems = i; /* only [0, i) are populated -- keeps destroy correct */
      wuss_menu_destroy(m);
      return result_OOM;
    }

    if (src->items[i].submenu != NULL)
    {
      wuss_menu_t *child;

      rc = menu_deep_copy(src->items[i].submenu, &child);
      if (rc != result_OK)
      {
        m->nitems = i + 1;
        wuss_menu_destroy(m);
        return rc;
      }
      items[i].submenu = child;
    }
  }

  *out = m;
  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* A menu under construction: a growable wuss_menu_item_t array plus the
 * pending '|' dash flag owed to the next-added item. */
typedef struct
{
  wuss_menu_item_t *items;
  int               n;
  int               cap;
  int               pending_dash;
}
Building;

static result_t build_emit(Building    *b,
                           const char  *text,
                           unsigned int flags,
                           wuss_menu_t *submenu)
{
  wuss_menu_item_t *it;
  char             *copy;

  if (array_grow((void **) &b->items, sizeof(*b->items),
                 b->n, &b->cap, 1, 4))
    return result_OOM;

  copy = strdup(text ? text : "");
  if (copy == NULL)
    return result_OOM;

  it = &b->items[b->n++];
  it->text    = copy;
  it->flags   = flags;
  it->submenu = submenu;
  it->window  = NULL; /* array_grow leaves these uninitialised otherwise */
  it->swatch  = wuss_NO_BACKGROUND;
  return result_OK;
}

static result_t build_add(Building    *b,
                          const char  *text,
                          unsigned int opt,
                          wuss_menu_t *submenu)
{
  unsigned int flags;

  flags = wuss_MENU_ITEM_NONE;

  /* '|' before this item draws a dashed rule above it: set DASHED on the item
   * itself, which stays an ordinary interactive row. */
  if (b->pending_dash)
  {
    b->pending_dash = 0;
    flags |= wuss_MENU_ITEM_DASHED;
  }

  if (opt & Tick)
    flags |= wuss_MENU_ITEM_TICKED;
  if (opt & Shade)
    flags |= wuss_MENU_ITEM_DISABLED;

  return build_emit(b, text, flags, submenu);
}

/* Seal a Building into a heap wuss_menu_t, transferring its item array. */
static wuss_menu_t *build_seal(Building *b)
{
  wuss_menu_t *m;

  m = malloc(sizeof(*m));
  if (m == NULL)
    return NULL;

  /* ponytail: the descriptor's { } title token is still discarded, so a
   * parser-built menu has no caption. Wire it through Building if that
   * changes. */
  m->title  = NULL;
  m->items  = b->items;
  m->nitems = b->n;

  b->items = NULL;
  b->n = b->cap = 0;

  return m;
}

/* Free an unsealed Building (error path): everything in it is ours. */
static void build_discard(Building *b)
{
  int i;

  for (i = 0; i < b->n; i++)
  {
    free((void *) b->items[i].text); /* discard const */
    wuss_menu_destroy((wuss_menu_t *) b->items[i].submenu);
  }
  free(b->items);
  b->items = NULL;
  b->n = b->cap = 0;
}

/* ----------------------------------------------------------------------- */

result_t wuss_menu_create_from_desc(wuss_menu_t **out, const char *desc, ...)
{
  Parser   parser;
  Building stack[WUSS_MENU_DESC_DEPTH];
  int      need_title[WUSS_MENU_DESC_DEPTH];
  char    *titles[WUSS_MENU_DESC_DEPTH];
  int      sp;
  va_list  ap;
  result_t rc;
  char     text[WUSS_MENU_DESC_TEXT];
  int      i;

  assert(out  != NULL);
  assert(desc != NULL);

  parser.p = desc;

  for (i = 0; i < WUSS_MENU_DESC_DEPTH; i++)
  {
    stack[i].items = NULL;
    stack[i].n = stack[i].cap = stack[i].pending_dash = 0;
    need_title[i] = 0;
    titles[i]     = NULL;
  }

  /* the descriptor's first token is the root menu's title, exactly as the
   * first token inside a { } is that submenu's -- so the same strings port
   * from PrivateEye unchanged */
  need_title[0] = 1;

  sp = 0;
  rc = result_OK;

  va_start(ap, desc);

  for (;;)
  {
    Token tok;

    tok = getnext(&parser);

    if (tok == End)
      break;

    if (tok == Error)
    {
      rc = result_BAD_ARG;
      break;
    }

    if (tok == Sep)
      continue;

    if (tok == DashSep)
    {
      stack[sp].pending_dash = 1;
      continue;
    }

    if (tok == Push)
    {
      if (sp + 1 >= WUSS_MENU_DESC_DEPTH)
      {
        rc = result_BAD_ARG; /* too deep */
        break;
      }
      sp++;
      need_title[sp] = 1; /* first token inside { } is a title, discard it */
      continue;
    }

    if (tok == Pop)
    {
      wuss_menu_t *sub;

      if (sp == 0)
      {
        rc = result_BAD_ARG; /* } without { */
        break;
      }

      sub = build_seal(&stack[sp]);
      if (sub == NULL)
      {
        rc = result_OOM;
        break;
      }
      sp--;

      if (stack[sp].n == 0)
      {
        wuss_menu_destroy(sub);
        rc = result_BAD_ARG; /* { } with no preceding item */
        break;
      }
      stack[sp].items[stack[sp].n - 1].submenu = sub;
      continue;
    }

    /* tok == Name */
    {
      char        *q;
      const char  *s;
      unsigned int opt;
      int          overflow;

      opt      = parser.flags;
      q        = text;
      overflow = 0;

      for (s = parser.start; s != parser.end; s++)
      {
        if (*s == '%' && (s + 1) != parser.end && s[1] == 's')
        {
          const char *arg;
          size_t      len;

          arg = va_arg(ap, const char *);
          len = strlen(arg);
          if ((size_t) (q - text) + len >= sizeof(text))
          {
            overflow = 1;
            break;
          }
          memcpy(q, arg, len);
          q += len;
          s++;
        }
        else
        {
          if ((size_t) (q - text) + 1 >= sizeof(text))
          {
            overflow = 1;
            break;
          }
          *q++ = *s;
        }
      }

      if (overflow)
      {
        rc = result_BAD_ARG;
        break;
      }
      *q = '\0';

      if (need_title[sp])
      {
        /* the menu's title token -- captured, not emitted as an item; kept
         * only for the root (sp 0), submenus get no caption */
        need_title[sp] = 0;
        if (sp == 0)
        {
          titles[0] = strdup(text);
          if (titles[0] == NULL)
          {
            rc = result_OOM;
            break;
          }
        }
        continue;
      }

      {
        wuss_menu_t *submenu;

        submenu = NULL;
        if (opt & SubMenu)
        {
          const wuss_menu_t *borrowed;

          borrowed = va_arg(ap, const wuss_menu_t *);
          rc = menu_deep_copy(borrowed, &submenu);
          if (rc != result_OK)
            break;
        }

        rc = build_add(&stack[sp], text, opt, submenu);
        if (rc != result_OK)
        {
          wuss_menu_destroy(submenu);
          break;
        }
      }
    }
  }

  va_end(ap);

  if (rc == result_OK && sp != 0)
    rc = result_BAD_ARG; /* unclosed { */

  if (rc != result_OK)
  {
    for (i = 0; i < WUSS_MENU_DESC_DEPTH; i++)
    {
      build_discard(&stack[i]);
      free(titles[i]);
    }
    return rc;
  }

  *out = build_seal(&stack[0]);
  if (*out == NULL)
  {
    build_discard(&stack[0]);
    free(titles[0]);
    return result_OOM;
  }

  (*out)->title = titles[0]; /* ownership transferred; NULL if none given */

  return result_OK;
}
