/* wuss/icon/writable.c -- editable text field icons */

#include <assert.h>
#include <string.h>

#include "base/utils.h"
#include "framebuf/bmfont.h"
#include "text/utf8.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

bmfont_t *wuss__icon_font(const wuss_t *wuss, const wuss_icon_t *icon)
{
  bmfont_t *font;

  font = wuss->fonts.fonts[wuss_ICON_FONT_OF(icon->spec.flags)];
  if (font == NULL)
    font = wuss->fonts.fonts[0];

  return font;
}

void wuss__writable_copy(char *buf, int size, const char *text)
{
  size_t len;
  int    trimmed;

  len = MIN(strlen(text), (size_t) size - 1);

  /* don't split a codepoint: back off over up to three continuation bytes */
  for (trimmed = 0;
       trimmed < 3 && len > 0 && ((unsigned char) text[len] & 0xC0) == 0x80;
       trimmed++)
    len--;

  memcpy(buf, text, len);
  buf[len] = '\0';
}

/* ----------------------------------------------------------------------- */

/* Caret x relative to the start of the text; 0 without a font. */
static int writable_caret_x(const wuss_t      *wuss,
                            const wuss_icon_t *icon,
                            int                index)
{
  bmfont_t *font;

  font = wuss__icon_font(wuss, icon);
  if (font == NULL)
    return 0;

  return (int) bmfont_caret_x(font, icon->spec.text, index, NULL);
}

/* Can "icon" take the caret? */
static int writable_is_live(const wuss_icon_t *icon)
{
  return icon->spec.type == wuss_ICON_TYPE_WRITABLE &&
         !(icon->spec.flags & (wuss_ICON_FLAGS_HIDDEN |
                               wuss_ICON_FLAGS_DISABLED));
}

/* The next live writable after "from" on "window" (the previous one when
 * "step" is -1), wrapping round; "from" itself when it is the only one. */
static wuss_icon_t *writable_neighbour(const wuss_window_t *window,
                                       wuss_icon_t         *from,
                                       int                  step)
{
  wuss_icon_t *it;
  int          n;
  int          i;
  int          j;

  /* ponytail: icon array order, which a wuss_icon_delete swap-remove can
   * reshuffle; add an explicit tab order if that ever matters */
  n = window->nicons;
  for (i = 0; i < n; i++)
    if (window->icons[i] == from)
      break;

  for (j = 1; j <= n; j++)
  {
    it = window->icons[((i + step * j) % n + n) % n];
    if (writable_is_live(it))
      return it;
  }

  return from;
}

/* Start of the word before "index": skip spaces, then non-spaces. */
static int writable_word_left(const char *text, int index)
{
  while (index > 0 && text[index - 1] == ' ')
    index--;
  while (index > 0 && text[index - 1] != ' ')
    index--;

  return index;
}

/* End of the word after "index": skip spaces, then non-spaces. */
static int writable_word_right(const char *text, int len, int index)
{
  while (index < len && text[index] == ' ')
    index++;
  while (index < len && text[index] != ' ')
    index++;

  return index;
}

/* The byte index of the codepoint containing "index", or "len" past it. */
static int writable_rune_start(const char *text, int len, int index)
{
  unsigned long codepoint;
  int           i;
  int           n;

  /* ponytail: a scan from the start, fine for field-sized strings */
  for (i = 0; i < len; i += n)
  {
    n = utf8_decode(text + i, len - i, &codepoint);
    if (i + n > index)
      break;
  }

  return i;
}

/* Tell the caret icon's task its text changed. */
static result_t writable_changed(wuss_t *wuss)
{
  wuss_event_t event;

  event.kind             = wuss_EVENT_ICON;
  event.data.icon.icon   = wuss->caret_icon;
  event.data.icon.action = wuss_MOUSE_UP;
  event.data.icon.button = wuss_BUTTON_NONE;
  event.data.icon.value  = wuss->caret_icon->value;
  return wuss__deliver(wuss->caret_window->task, wuss->caret_window, &event);
}

/* ----------------------------------------------------------------------- */

void wuss__caret_clear(wuss_t *wuss)
{
  if (wuss->caret_icon == NULL)
    return;

  wuss__icon_invalidate(wuss->caret_window, wuss->caret_icon);
  wuss->caret_icon   = NULL;
  wuss->caret_window = NULL;
  wuss->caret_index  = 0;
}

void wuss__writable_place_caret(wuss_window_t *window,
                                wuss_icon_t   *icon,
                                int            index)
{
  wuss_t *wuss;
  int     len;
  int     visible;
  int     x;

  wuss = window->wuss;

  if (wuss->caret_icon != icon)
    wuss__caret_clear(wuss);

  len   = (int) strlen(icon->spec.text);
  if (index < 0)
    index = len;
  else
    index = writable_rune_start(icon->spec.text, len, index);

  /* keep the caret in view, jumping a third of the field at a time so typing
   * at an edge doesn't scroll on every key */
  visible = MAX((icon->spec.bbox.x1 - icon->spec.bbox.x0) -
                2 * WUSS_WRITABLE_INSET, 1);
  x       = writable_caret_x(wuss, icon, index);
  if (index == 0)
    icon->text_scroll = 0;
  else if (x < icon->text_scroll)
    icon->text_scroll = MAX(x - visible / 3, 0);
  else if (x > icon->text_scroll + visible)
    icon->text_scroll = x - visible + visible / 3;

  wuss->caret_icon   = icon;
  wuss->caret_window = window;
  wuss->caret_index  = index;

  wuss__icon_invalidate(window, icon);
}

int wuss__writable_index_for_x(const wuss_t      *wuss,
                               const wuss_icon_t *icon,
                               int                doc_x)
{
  bmfont_t *font;
  int       len;
  int       index;

  font = wuss__icon_font(wuss, icon);
  len  = (int) strlen(icon->spec.text);
  if (font == NULL)
    return len;

  bmfont_find_caret(font, icon->spec.text, len, NULL,
                    doc_x - (icon->spec.bbox.x0 + WUSS_WRITABLE_INSET) +
                    icon->text_scroll,
                    &index, NULL);
  return index;
}

result_t wuss__writable_key(wuss_t              *wuss,
                            int                  code,
                            wuss_key_modifiers_t modifiers,
                            int                 *claimed)
{
  wuss_window_t *window;
  wuss_icon_t   *icon;
  char          *text;
  int            len;
  int            index;
  int            step;
  unsigned long  codepoint;
  int            n;
  char           utf8[4];

  *claimed = 0;

  window = wuss->caret_window;
  icon   = wuss->caret_icon;
  if (icon == NULL || (modifiers & wuss_KEY_MOD_ALT))
    return result_OK;

  text  = (char *) icon->spec.text; /* owned buffer of spec.u.writable.size */
  len   = (int) strlen(text);
  index = wuss->caret_index;

  if (modifiers & wuss_KEY_MOD_CTRL)
  {
    switch (code)
    {
    case 'u':
    case 'U':
      *claimed = 1;
      if (len == 0)
        return result_OK;

      text[0] = '\0';
      wuss__writable_place_caret(window, icon, 0);
      return writable_changed(wuss);

    case wuss_KEY_LEFT:
      *claimed = 1;
      wuss__writable_place_caret(window, icon, 0);
      return result_OK;

    case wuss_KEY_RIGHT:
      *claimed = 1;
      wuss__writable_place_caret(window, icon, len);
      return result_OK;

    default:
      return result_OK;
    }
  }

  switch (code)
  {
  case 9: /* Tab */
    *claimed = 1;
    step     = (modifiers & wuss_KEY_MOD_SHIFT) ? -1 : 1;
    wuss__writable_place_caret(window,
                               writable_neighbour(window, icon, step),
                               -1);
    return result_OK;

  case wuss_KEY_LEFT:
    *claimed = 1;
    if (modifiers & wuss_KEY_MOD_SHIFT)
      index = writable_word_left(text, index);
    else if (index > 0)
      index = utf8_prev(text, index);
    wuss__writable_place_caret(window, icon, index);
    return result_OK;

  case wuss_KEY_RIGHT:
    *claimed = 1;
    if (modifiers & wuss_KEY_MOD_SHIFT)
      index = writable_word_right(text, len, index);
    else if (index < len)
      index += utf8_decode(text + index, len - index, &codepoint);
    wuss__writable_place_caret(window, icon, index);
    return result_OK;

  case wuss_KEY_HOME:
    *claimed = 1;
    wuss__writable_place_caret(window, icon, 0);
    return result_OK;

  case wuss_KEY_END:
    *claimed = 1;
    wuss__writable_place_caret(window, icon, len);
    return result_OK;

  case 8: /* Backspace */
    *claimed = 1;
    if (index == 0)
      return result_OK;

    n = index - utf8_prev(text, index);
    memmove(text + index - n, text + index, (size_t) (len - index + 1));
    wuss__writable_place_caret(window, icon, index - n);
    return writable_changed(wuss);

  case wuss_KEY_DELETE:
    *claimed = 1;
    if (index == len)
      return result_OK;

    n = utf8_decode(text + index, len - index, &codepoint);
    memmove(text + index, text + index + n, (size_t) (len - index - n + 1));
    wuss__writable_place_caret(window, icon, index);
    return writable_changed(wuss);

  default:
    /* printable codepoints only: not C0/C1 controls or DEL, and not the key
     * constants above U+10FFFF, which utf8_encode refuses */
    if (code < 0x20 || (code >= 0x7F && code <= 0x9F))
      return result_OK;

    n = utf8_encode((unsigned long) code, utf8);
    if (n == 0)
      return result_OK;

    *claimed = 1;
    if (len + n >= icon->spec.u.writable.size)
      return result_OK; /* full */

    memmove(text + index + n, text + index, (size_t) (len - index + 1));
    memcpy(text + index, utf8, (size_t) n);
    wuss__writable_place_caret(window, icon, index + n);
    return writable_changed(wuss);
  }
}

/* ----------------------------------------------------------------------- */

result_t wuss_icon_set_caret(wuss_window_t *window,
                             wuss_icon_t   *icon,
                             int            index)
{
  result_t rc;

  assert(window != NULL);

  if (icon == NULL)
  {
    if (window->wuss->caret_window == window)
      wuss__caret_clear(window->wuss);
    return result_OK;
  }

  if (!writable_is_live(icon))
    return result_BAD_ARG;

  rc = wuss_set_focus(window->wuss, window);
  if (rc != result_OK)
    return rc;

  wuss__writable_place_caret(window, icon, index);

  return result_OK;
}
