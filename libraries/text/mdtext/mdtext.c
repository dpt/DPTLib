/* text/mdtext/mdtext.c -- lay out and draw Markdown or plain text in a bmfont */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "base/utils.h"
#include "text/mdtext.h"
#include "text/utf8.h"

#define MDTEXT_MAX_SPANS 128 /* styled runs are few; overflow is dropped */

/* ----------------------------------------------------------------------- */

/* fixed tint of each style, independent of any palette */
static colour_t mdtext_span_colour(mdspans_style_t style)
{
  switch (style)
  {
  case mdspans_HEADER: return colour_rgb(0x20, 0x60, 0xd0);
  case mdspans_BOLD:   return colour_rgb(0xd0, 0x20, 0x20);
  case mdspans_ITALIC: return colour_rgb(0x20, 0x90, 0x40);
  default:             return colour_rgb(0x90, 0x40, 0xc0);
  }
}

/* the face a run is drawn in: a bold span's in the bold face where there is
 * one, everything else in the regular face */
static bmfont_t *mdtext_run_font(const mdtext_t *mdtext,
                                 const mdspan_t *span)
{
  return span != NULL && span->style == mdspans_BOLD && mdtext->bold != NULL
         ? mdtext->bold : mdtext->font;
}

/* Split off the next run of up to len bytes at byte offset off of the text:
 * either part of one span, or unstyled text up to the next span. Returns the
 * span, or NULL for unstyled text, and sets *run_len. Spans are sorted and
 * never overlap, so their ends rise too: span_cursor is stepped back or
 * forward to the first span ending after off. */
static const mdspan_t *mdtext_run(mdtext_t *mdtext,
                                  int       off,
                                  int       len,
                                  int      *run_len)
{
  const mdspan_t *spans;
  int             n;
  int             c;
  const mdspan_t *s;

  spans = mdtext->spans;
  n     = mdtext->nspans;

  c = MIN(mdtext->span_cursor, n);
  while (c > 0 && spans[c - 1].start + spans[c - 1].len > off)
    c--;
  while (c < n && spans[c].start + spans[c].len <= off)
    c++;
  mdtext->span_cursor = c;

  *run_len = len;
  if (c == n)
    return NULL; /* past the last span */

  s = &spans[c];
  if (off >= s->start)
  {
    *run_len = MIN(len, s->start + s->len - off);
    return s;
  }

  *run_len = MIN(len, s->start - off);
  return NULL;
}

/* bmtext_measure_fn over the text: as bmfont_measure, but measuring each run
 * in the face it is drawn in, so bold text wraps and aligns at its own
 * width */
static result_t mdtext_measure(const char     *text,
                               int             len,
                               bmfont_width_t  target_width,
                               int            *split_point,
                               bmfont_width_t *actual_width,
                               void           *opaque)
{
  result_t        rc;
  mdtext_t       *mdtext = opaque;
  int             cursor;
  bmfont_width_t  width;
  const mdspan_t *span;
  int             run_len;
  int             split;
  bmfont_width_t  run_width;

  rc     = result_OK;
  cursor = 0;
  width  = 0;
  while (cursor < len)
  {
    span = mdtext_run(mdtext, (int) (text - mdtext->text) + cursor,
                      len - cursor, &run_len);
    rc   = bmfont_measure(mdtext_run_font(mdtext, span), text + cursor,
                          run_len, &mdtext->spacing, target_width - width,
                          &split, &run_width);
    if (rc != result_OK)
      break;

    cursor += split;
    width  += run_width;
    if (split < run_len)
      break; /* the rest does not fit */
  }

  *split_point  = cursor;
  *actual_width = width;
  return rc;
}

/* bmtext_layout_measured wraps at spaces only, so lay out each
 * '\n'-separated segment on its own. A blank segment still makes one empty
 * line, so paragraph gaps survive. Returns the number of lines written,
 * capped at max. */
static int mdtext_layout_lines(mdtext_t      *mdtext,
                               int            wrap_width,
                               bmtext_line_t *lines,
                               int            max)
{
  int         nlines;
  const char *seg;
  const char *nl;
  int         seglen;

  nlines = 0;
  seg    = mdtext->text;
  while (nlines < max)
  {
    nl     = strchr(seg, '\n');
    seglen = nl != NULL ? (int) (nl - seg) : (int) strlen(seg);

    if (seglen == 0)
    {
      lines[nlines].str = seg;
      lines[nlines].len = 0;
      nlines++;
    }
    else
    {
      nlines += bmtext_layout_measured(mdtext_measure, mdtext, seg, seglen,
                                       wrap_width, lines + nlines,
                                       max - nlines);
    }

    if (nl == NULL)
      break;
    seg = nl + 1;
  }

  return nlines;
}

/* ----------------------------------------------------------------------- */

result_t mdtext_set(mdtext_t *mdtext, const char *src, int len, int markdown)
{
  result_t  rc;
  char     *text;
  mdspan_t *spans;
  int       nspans;

  spans  = NULL;
  nspans = 0;
  if (markdown)
  {
    rc = mdspans_parse(src, len, MDTEXT_MAX_SPANS, &text, &spans, &nspans);
    if (rc != result_OK)
      return rc;
  }
  else
  {
    text = malloc((size_t) len + 1);
    if (text == NULL)
      return result_OOM;
    memcpy(text, src, (size_t) len);
    text[len] = '\0';
  }

  mdtext_free(mdtext);
  mdtext->text   = text;
  mdtext->spans  = spans;
  mdtext->nspans = nspans;
  return result_OK;
}

result_t mdtext_layout(mdtext_t *mdtext, int wrap_width)
{
  int            relaid;
  size_t         max;
  bmtext_line_t *lines;
  int           *dx;
  int            i;
  int            split;
  bmfont_width_t width;

  if (mdtext->text == NULL)
    return result_OK;

  relaid = 0;
  if (mdtext->lines == NULL || wrap_width != mdtext->laid_wrap ||
      mdtext->font != mdtext->laid_font || mdtext->bold != mdtext->laid_bold ||
      memcmp(&mdtext->spacing, &mdtext->laid_spacing,
             sizeof(mdtext->spacing)) != 0)
  {
    /* every line holds at least one byte, plus one empty line per newline,
     * so strlen + 1 bounds the count */
    max   = strlen(mdtext->text) + 1;
    lines = malloc(max * sizeof(*lines));
    dx    = malloc(max * sizeof(*dx));
    if (lines == NULL || dx == NULL)
    {
      free(lines);
      free(dx);
      return result_OOM;
    }

    free(mdtext->lines);
    free(mdtext->dx);
    mdtext->lines  = lines;
    mdtext->dx     = dx;
    mdtext->nlines = mdtext_layout_lines(mdtext, wrap_width, lines, (int) max);

    mdtext->laid_wrap    = wrap_width;
    mdtext->laid_font    = mdtext->font;
    mdtext->laid_bold    = mdtext->bold;
    mdtext->laid_spacing = mdtext->spacing;
    relaid               = 1;
  }

  if (!relaid && mdtext->align == mdtext->laid_align)
    return result_OK;

  /* per-line indent for the alignment; a line wider than the wrap width (a
   * single unbreakable word) measures as the wrap width, so it stays at the
   * left edge */
  for (i = 0; i < mdtext->nlines; i++)
  {
    mdtext->dx[i] = 0;
    if (mdtext->align == mdtext_ALIGN_LEFT || mdtext->lines[i].len == 0)
      continue; /* bmfont_measure refuses an empty run */

    if (mdtext_measure(mdtext->lines[i].str, mdtext->lines[i].len, wrap_width,
                       &split, &width, mdtext) != result_OK)
      continue;

    mdtext->dx[i] = wrap_width - width;
    if (mdtext->align == mdtext_ALIGN_CENTRE)
      mdtext->dx[i] /= 2;
  }
  mdtext->laid_align = mdtext->align;

  return result_OK;
}

int mdtext_nlines(const mdtext_t *mdtext)
{
  return mdtext->lines != NULL ? mdtext->nlines : 0;
}

int mdtext_line_height(const mdtext_t *mdtext)
{
  int height;

  bmfont_get_info(mdtext->font, NULL, &height, NULL, NULL);

  return height + mdtext->leading;
}

void mdtext_draw(mdtext_t *mdtext,
                 screen_t *scr,
                 point_t   origin,
                 colour_t  fg,
                 colour_t  bg)
{
  int             line_height;
  int             ascent;
  int             first, last;
  int             i;
  point_t         pos;
  const char     *str;
  int             len;
  int             off;
  int             cursor;
  const mdspan_t *span;
  int             run_len;

  if (mdtext->lines == NULL)
    return;

  line_height = mdtext_line_height(mdtext);
  bmfont_get_info(mdtext->font, NULL, NULL, &ascent, NULL);

  /* only the lines the clip touches */
  first = (scr->clip.y0 - origin.y) / line_height;
  last  = DIVIDE_ROUNDING_UP(scr->clip.y1 - origin.y, line_height);
  first = CLAMP(first, 0, mdtext->nlines);
  last  = CLAMP(last, first, mdtext->nlines);

  for (i = first; i < last; i++)
  {
    str    = mdtext->lines[i].str;
    len    = mdtext->lines[i].len;
    off    = (int) (str - mdtext->text);
    cursor = 0;
    pos.x  = origin.x + mdtext->dx[i];
    pos.y  = origin.y + i * line_height + ascent;

    while (cursor < len)
    {
      span = mdtext_run(mdtext, off + cursor, len - cursor, &run_len);
      bmfont_draw(mdtext_run_font(mdtext, span), scr, str + cursor, run_len,
                  span != NULL ? mdtext_span_colour(span->style) : fg, bg,
                  &mdtext->spacing, &pos, &pos);
      cursor += run_len;
    }
  }
}

int mdtext_caret_pos(mdtext_t *mdtext, int index, point_t *pos)
{
  int             ascent;
  int             i;
  int             off;
  int             cursor;
  int             x;
  const mdspan_t *span;
  int             run_len;
  int             split;
  bmfont_width_t  width;

  if (mdtext->lines == NULL || mdtext->nlines == 0)
    return 0;

  bmfont_get_info(mdtext->font, NULL, NULL, &ascent, NULL);

  index = CLAMP(index, 0, (int) strlen(mdtext->text));

  /* the last line starting at or before index */
  for (i = mdtext->nlines - 1; i > 0; i--)
    if (mdtext->lines[i].str - mdtext->text <= index)
      break;

  off   = (int) (mdtext->lines[i].str - mdtext->text);
  index = MIN(index - off, mdtext->lines[i].len);

  /* whole runs advance in full; the run holding the caret places it as
   * bmfont_caret_x does */
  x      = 0;
  cursor = 0;
  while (cursor < index)
  {
    span = mdtext_run(mdtext, off + cursor, index - cursor, &run_len);
    if (cursor + run_len == index)
    {
      x += bmfont_caret_x(mdtext_run_font(mdtext, span),
                          mdtext->lines[i].str + cursor, run_len,
                          &mdtext->spacing);
      break;
    }

    if (bmfont_measure(mdtext_run_font(mdtext, span),
                       mdtext->lines[i].str + cursor, run_len,
                       &mdtext->spacing, INT_MAX, &split,
                       &width) != result_OK)
      return 0;

    x      += width;
    cursor += run_len;
  }

  pos->x = mdtext->dx[i] + x;
  pos->y = i * mdtext_line_height(mdtext) + ascent;
  return 1;
}

int mdtext_find_caret(mdtext_t *mdtext, point_t pos, int *index)
{
  int             i;
  const char     *str;
  int             len;
  int             off;
  int             x;
  int             cursor;
  const mdspan_t *span;
  int             run_len;
  bmfont_t       *font;
  int             split;
  bmfont_width_t  width;
  int             caret;

  if (mdtext->lines == NULL || mdtext->nlines == 0)
    return 0;

  i   = CLAMP(pos.y / mdtext_line_height(mdtext), 0, mdtext->nlines - 1);
  str = mdtext->lines[i].str;
  len = mdtext->lines[i].len;
  off = (int) (str - mdtext->text);
  x   = pos.x - mdtext->dx[i];

  /* skip the runs wholly left of x; the last run takes any x beyond it */
  cursor = 0;
  while (cursor < len)
  {
    span = mdtext_run(mdtext, off + cursor, len - cursor, &run_len);
    font = mdtext_run_font(mdtext, span);
    if (bmfont_measure(font, str + cursor, run_len, &mdtext->spacing, INT_MAX,
                       &split, &width) != result_OK)
      return 0;

    if (x < width || cursor + run_len == len)
    {
      bmfont_find_caret(font, str + cursor, run_len, &mdtext->spacing, x,
                        &caret, NULL);
      cursor += caret;
      break;
    }

    x      -= width;
    cursor += run_len;
  }

  /* a hard break: this index would draw on the next line */
  if (len > 0 && cursor == len && i + 1 < mdtext->nlines &&
      mdtext->lines[i + 1].str == str + len)
    cursor = utf8_prev(str, len);

  *index = off + cursor;
  return 1;
}

void mdtext_free(mdtext_t *mdtext)
{
  free(mdtext->text);
  free(mdtext->spans);
  free(mdtext->lines);
  free(mdtext->dx);
  mdtext->text        = NULL;
  mdtext->spans       = NULL;
  mdtext->nspans      = 0;
  mdtext->span_cursor = 0;
  mdtext->lines       = NULL;
  mdtext->dx          = NULL;
  mdtext->nlines      = 0;
}
